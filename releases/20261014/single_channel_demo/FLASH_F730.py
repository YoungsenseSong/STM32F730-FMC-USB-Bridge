"""Program the reviewed F730 USB candidate only after matching the full backup."""
import hashlib
import json
import logging
import time
import sys
from pathlib import Path
from intelhex import IntelHex
from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
from pyocd.coresight.ap import AccessPort, APv1Address
from pyocd.flash.file_programmer import FileProgrammer
from pyocd.core.memory_map import MemoryType, RamRegion
from pyocd.target.pack.flash_algo import PackFlashAlgo
from pyocd.target.pack.flm_region_builder import FlmFlashRegionBuilder
logging.basicConfig(level=logging.INFO)
root = Path(__file__).resolve().parent
source = root/'f730' if (root/'f730/CMakeLists.txt').exists() else root
image = source/'images/F730Bridge.hex'
image_hash = hashlib.sha256(image.read_bytes()).hexdigest()
backup = root/'recovery/before_64k.bin'
assert len(backup.read_bytes()) == 65536
assert hashlib.sha256(backup.read_bytes()).hexdigest() == '8c4cc9d9daec93ec60e83773f64ea71c1d04adc44eb859870d597ea460893b3d'
ih = IntelHex(str(image))
assert all(0x08000000 <= start < end <= 0x08010000 for start,end in ih.segments())
s = ConnectHelper.session_with_chosen_probe(
    unique_id='56FF72065065545741451787', target_override='stm32f730v8',
    frequency=100000, connect_mode='attach', auto_unlock=False, chip_erase='sector',
    resume_on_disconnect=True, blocking=False,
    options={'cache.enable_memory':False,'cache.enable_register':False,'cache.read_code_from_elf':False,
             'pack.debug_sequences.enable':False})
r = dict(image=str(image), image_sha256=image_hash, backup=str(backup), flash_write_started=False,
         flashed=False, verified=False, rolled_back=False)
ap = None
try:
    # Use the pack's allocator with a real 64 KiB DTCM range, before flash objects are created.
    # This changes this session only, not the installed pack or firmware inputs.
    algo_regions = []
    for region in s.target.memory_map.iter_matching_regions(type=MemoryType.FLASH):
        assert isinstance(region.flm, PackFlashAlgo), 'Expected the installed pack FLM'
        old_algo = region.algo
        region._RAMstart = 0x20000000
        region._RAMsize = 0x10000
        assert FlmFlashRegionBuilder(s.target,s.target.memory_map).finalise_region(region)
        assert region.algo is not None and region.sector_size > 0 and region.page_size > 0
        algo_regions.append(dict(region=region.name, start=hex(region.start),
            old_load_address=hex(old_algo['load_address']) if old_algo else None,
            algo={k: v for k,v in region.algo.items() if k != 'instructions'},
            instruction_bytes=len(region.algo['instructions'])*4,
            sector_size=region.sector_size,page_size=region.page_size))
    assert algo_regions
    r['algo_regions'] = algo_regions
    if '--inspect-only' in sys.argv:
        print(json.dumps(algo_regions,indent=2),flush=True)
        sys.exit(0)
    s.open(init_board=False)
    s.target.dp.connect()
    ap = AccessPort.create(s.target.dp,APv1Address(0))
    assert ap.read32(0xE0042000) & 0xFFF == 0x452
    ap.write32(0xE000EDF0,0xA05F0001)
    ap.write32(0xE000EDFC,0x01000001)
    ap.write32(0xE000ED0C,0x05FA0004)
    s.target.flush()
    print('EXPLICIT_SYSRESET_CATCH_REQUESTED',flush=True)
    deadline = time.monotonic()+5
    while time.monotonic()<deadline:
        if ap.read32(0xE000EDF0) & (1 << 17):
            break
        time.sleep(.1)
    else:
        raise RuntimeError('No S_HALT after manual release')
    freeze = ap.read32(0xE0042008)
    ap.write32(0xE0042008,freeze | (1 << 12))
    before = bytearray()
    for offset in range(0,65536,256):
        before.extend(ap.read_memory_block8(0x08000000+offset,256))
    assert bytes(before) == backup.read_bytes(), 'Current full flash differs from rollback backup'
    (root/'live_before_slow_flash_64k.bin').write_bytes(before)
    r['backup_confirmed'] = True
    r['backup_sha256'] = hashlib.sha256(before).hexdigest()
    # The DP is already connected. Re-entering SWD (and the pack DebugCoreStart)
    # during ordinary board.init let the application run again in v2.
    # Keep this manual reset-catch connection and use generic core initialization.
    original_sequence = s.target.create_init_sequence
    def retained_connection_sequence():
        sequence = original_sequence()
        sequence.remove_task('dp_init')
        return sequence
    s.target.create_init_sequence = retained_connection_sequence
    s.board.init()
    s.target.reset_and_halt(Target.ResetType.SYSRESETREQ)
    s.target.write32(0xE000E010,0)
    s.target.write_core_register('primask',1)
    s.target.write32(0xE0042008,s.target.read32(0xE0042008)|(1<<12))
    r['after_init_dhcsr'] = hex(s.target.read32(0xE000EDF0))
    assert s.target.read32(0xE000EDF0) & (1 << 17), 'Core did not remain halted'
    # Keep writes small so STLink does not issue a long burst across RAM boundaries.
    original_write32 = s.target.write_memory_block32
    def small_write32(address, data):
        for off in range(0,len(data),64):
            original_write32(address+off*4, data[off:off+64])
    s.target.write_memory_block32 = small_write32
    for region in s.target.memory_map.iter_matching_regions(type=MemoryType.FLASH):
        algo = region.algo
        s.target.write_memory_block32(algo['load_address'],algo['instructions'])
        actual_words = []
        for off in range(0,len(algo['instructions']),64):
            actual_words.extend(s.target.read_memory_block32(algo['load_address']+off*4,
                                min(64,len(algo['instructions'])-off)))
        assert actual_words == algo['instructions'], 'RAM flash algorithm readback mismatch'
    r['ram_preflight_ok'] = True
    print('BACKUP_UNCHANGED_AND_DTCM_ALGORITHM_READBACK_OK',flush=True)
    r['flash_write_started'] = True
    FileProgrammer(s,chip_erase='sector',smart_flash=False,trust_crc=False,keep_unwritten=True).program(str(image))
    r['flashed'] = True
    for start,end in ih.segments():
        actual = bytearray()
        for address in range(start,end,256):
            actual.extend(s.target.read_memory_block8(address,min(256,end-address)))
        expected = bytes(ih.tobinarray(start=start,end=end-1))
        assert bytes(actual) == expected, 'Candidate segment readback mismatch'
        (root/'gpio_serial_segment_readback.bin').write_bytes(actual)
    expected_full = bytearray(backup.read_bytes())
    for start,end in ih.segments():
        expected_full[start-0x08000000:end-0x08000000] = bytes(ih.tobinarray(start=start,end=end-1))
    actual_full = bytearray()
    for offset in range(0,65536,256):
        actual_full.extend(s.target.read_memory_block8(0x08000000+offset,256))
    assert actual_full == expected_full, 'Full flash, including unwritten tail, differs'
    (root/'gpio_serial_full_readback.bin').write_bytes(actual_full)
    r['full_flash_sha256'] = hashlib.sha256(actual_full).hexdigest()
    r['verified'] = True
    print('CANDIDATE_FLASH_READBACK_OK',flush=True)
    s.target.write32(0xE000EDFC,0x01000000)
    s.target.reset(Target.ResetType.SYSRESETREQ)
    s.target.resume()
    s.target.write32(0xE000EDF0,0xA05F0000)
    print('CANDIDATE_RUNNING',flush=True)
except Exception as e:
    r['error'] = type(e).__name__ + ': ' + str(e)
    if r['flash_write_started']:
        try:
            s.target.reset_and_halt(Target.ResetType.SYSRESETREQ)
            s.target.write32(0xE000E010,0)
            s.target.write_core_register('primask',1)
            s.target.write32(0xE0042008,s.target.read32(0xE0042008)|(1<<12))
            FileProgrammer(s,chip_erase='sector',smart_flash=False,trust_crc=False,keep_unwritten=True).program(str(backup),base_address=0x08000000)
            restored = bytearray()
            for off in range(0,65536,256):
                restored.extend(s.target.read_memory_block8(0x08000000+off,256))
            assert restored == backup.read_bytes(), 'Rollback readback mismatch'
            r['rolled_back'] = True
            s.target.write32(0xE000EDFC,0x01000000)
            s.target.reset(Target.ResetType.SYSRESETREQ)
            s.target.resume()
        except Exception as rollback_error:
            r['rollback_error'] = str(rollback_error)
    raise
finally:
    if ap is not None and not r['flash_write_started']:
        try:
            ap.write32(0xE000EDFC,0x01000000)
            ap.write32(0xE000EDF0,0xA05F0000)
        except Exception as e:
            r['cleanup_error'] = str(e)
    (root/('program_gpio_serial_metadata.json' if '--inspect-only' in sys.argv else 'program_gpio_serial.json')).write_text(json.dumps(r,indent=2),encoding='utf-8')
    s.close()

