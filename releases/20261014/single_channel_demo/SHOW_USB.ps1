param(
    [string]$Port = 'COM6',
    [double]$Seconds = 0,
    [string]$LogPath = ''
)
$ErrorActionPreference = 'Stop'
if ($Seconds -lt 0) { throw 'Seconds must be zero (continuous) or positive.' }
if (-not ('LiveUsbDecoder' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;

public class LiveUsbDecoder {
    private readonly List<byte> buffer = new List<byte>();
    private uint? packet, transport;
    private ushort? radio;
    public long Blocks, Records, Samples, UsbGaps, TransportGaps, RadioGaps, Discarded;
    public uint LastBlock, LastTransport, SensorMs;
    public ushort RadioSequence;
    public short[] Raw = new short[0];
    public string Error;
    static ushort U16(byte[] a, int o) { return BitConverter.ToUInt16(a,o); }
    static uint U32(byte[] a, int o) { return BitConverter.ToUInt32(a,o); }
    static ulong U64(byte[] a, int o) { return BitConverter.ToUInt64(a,o); }
    static void Require(bool value, string error) {
        if (!value) throw new InvalidOperationException(error);
    }
    static uint Crc32(byte[] a, int offset, int length) {
        uint crc = 0xffffffffu;
        for (int i=offset; i<offset+length; i++) {
            crc ^= a[i];
            for (int bit=0; bit<8; bit++)
                crc = (crc & 1) != 0 ? (crc >> 1)^0xedb88320u : crc >> 1;
        }
        return crc ^ 0xffffffffu;
    }
    static ushort Crc16(byte[] a, int offset, int length) {
        ushort crc = 0xffff;
        for (int i=offset; i<offset+length; i++) {
            crc ^= (ushort)(a[i] << 8);
            for (int bit=0; bit<8; bit++)
                crc = (ushort)(((crc & 0x8000) != 0) ? (crc << 1)^0x1021 : crc << 1);
        }
        return crc;
    }
    void ValidateBlock(byte[] a) {
        int b = 24;
        Require(a.Length >= b+64, "Block too short");
        Require(U32(a,b)==0x31425046 && U16(a,b+4)==1 && U16(a,b+6)==64, "Block identity");
        uint bytes=U32(a,b+16), mask=U32(a,b+20);
        Require(bytes>0 && bytes<=32736 && bytes%248==0 && a.Length==b+64+bytes, "Block length");
        Require(U32(a,b+8)==U32(a,12), "USB/block sequence mismatch");
        uint count=0, actualMask=0;
        for (int ch=0; ch<4; ch++) {
            uint n=U32(a,b+40+ch*4);
            Require(n<=132, "Channel record count");
            count+=n;
            if(n>0) actualMask|=(uint)(1<<ch);
        }
        Require(count==bytes/248 && mask==actualMask && (mask & ~15u)==0, "Channel/count mismatch");
        Require(U64(a,b+32)>=U64(a,b+24), "Block tick order");
        Require((U32(a,b+56) & ~8u)==0, "Block error flags");
        Require(Crc32(a,b+64,(int)bytes)==U32(a,b+60), "Block CRC32");
        // Validate every record before adding any of this block to the counters.
        for (int r=b+64; r<a.Length; r+=248) {
            Require(U32(a,r)==0x3146524e && a[r+4]==1 && U16(a,r+6)==1, "Record identity");
            Require(U16(a,r+36)==204 && Crc16(a,r,38)==U16(a,r+38), "Record header CRC16");
            Require(Crc32(a,r+40,204)==U32(a,r+244), "Record payload CRC32");
            Require(U16(a,r+40)==0xa55a && U16(a,r+44)>0 && U16(a,r+44)<=96, "RF frame identity");
            Require((U16(a,r+46) & 3)==2, "Not a real MEMS sample");
        }
        List<short> raw = new List<short>();
        for (int r=b+64; r<a.Length; r+=248) {
            uint seq=U32(a,r+8);
            if (transport.HasValue) {
                uint delta=unchecked(seq-transport.Value-1);
                TransportGaps += delta<0x80000000u ? delta : 1;
            }
            transport=seq;
            ushort rf=U16(a,r+42);
            if(radio.HasValue) {
                ushort delta=unchecked((ushort)(rf-radio.Value-1));
                RadioGaps += delta<0x8000 ? delta : 1;
            }
            radio=rf;
            RadioSequence=rf;
            SensorMs=U32(a,r+48);
            int n=U16(a,r+44);
            for(int i=0; i<n; i++) raw.Add(BitConverter.ToInt16(a,r+52+i*2));
            Samples+=n;
            LastTransport=seq;
        }
        Raw=raw.ToArray();
        LastBlock=U32(a,b+8);
        Blocks++;
        Records+=count;
    }
    public void Feed(byte[] data, int length) {
        if(Error!=null) return;
        for(int i=0; i<length; i++) buffer.Add(data[i]);
        try {
            while(buffer.Count>=4) {
                int offset=-1;
                for(int i=0; i<=buffer.Count-4; i++)
                    if(buffer[i]==0x55 && buffer[i+1]==0x42 && buffer[i+2]==0x52 && buffer[i+3]==0x31) { offset=i; break; }
                if(offset<0) {
                    int drop=buffer.Count-3;
                    Discarded+=drop; buffer.RemoveRange(0,drop); return;
                }
                if(offset>0) { Discarded+=offset; buffer.RemoveRange(0,offset); }
                if(buffer.Count<24) return;
                byte[] header=buffer.GetRange(0,24).ToArray();
                Require(U16(header,4)==1 && U16(header,6)>=1 && U16(header,6)<=4, "USB header");
                uint payload=U32(header,16);
                Require(payload<=32800, "USB payload too long");
                int frameBytes=24+(int)payload;
                if(buffer.Count<frameBytes) return;
                byte[] frame=buffer.GetRange(0,frameBytes).ToArray();
                Require(Crc32(frame,24,(int)payload)==U32(frame,20), "USB payload CRC32");
                if(U16(frame,6)==1) ValidateBlock(frame);
                uint seq=U32(frame,8);
                if(packet.HasValue && seq!=unchecked(packet.Value+1)) UsbGaps++;
                packet=seq;
                buffer.RemoveRange(0,frameBytes);
            }
        } catch(Exception ex) { Error=ex.Message; }
    }
}
'@
}

# The existing local viewer owns COM6; release its worker if it is active.
try {
    $viewer = Invoke-RestMethod 'http://127.0.0.1:8765/state' -TimeoutSec 1
    if ($viewer.running) {
        Invoke-WebRequest 'http://127.0.0.1:8765/stop' -Method Post -UseBasicParsing -TimeoutSec 2 | Out-Null
        Start-Sleep -Milliseconds 400
    }
} catch { }

$serial = New-Object System.IO.Ports.SerialPort $Port,115200,None,8,One
$serial.ReadTimeout = 200
$serial.ReadBufferSize = 65536
$decoder = New-Object LiveUsbDecoder
$buffer = New-Object byte[] 16384
$clock = [System.Diagnostics.Stopwatch]::StartNew()
$lastBlocks = 0
$lastPrint = -5.0
$lineCount = 0
$lastStatus = 'WAITING'
$saved = $null
$exitCode = 0
if ($LogPath) {
    $saved = New-Object System.IO.StreamWriter ([System.IO.Path]::GetFullPath($LogPath)),$false
    $saved.AutoFlush = $true
}
function Show-Line([string]$Line, [ConsoleColor]$Color) {
    Write-Host $Line -ForegroundColor $Color
    if ($saved) { $saved.WriteLine($Line) }
}
try {
    $serial.Open()
    Show-Line 'LIVE MEMS USB DEMO | IIM -> nRF TX -> RF -> nRF RX -> FPGA -> F730 -> PC' Cyan
    Show-Line ("Direct USB: {0} | USB FS CDC | Ctrl+C to stop | signed raw values" -f $Port) Cyan
    Show-Line 'USB + block + record CRC checked; RF gap includes radio/queue frame loss.' DarkGray
    while ($Seconds -eq 0 -or $clock.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            $n = $serial.Read($buffer,0,[Math]::Min($buffer.Length,[Math]::Max(1,$serial.BytesToRead)))
            if ($n -gt 0) { $decoder.Feed($buffer,$n) }
        } catch [System.TimeoutException] { }
        if ($decoder.Error) { throw ('Validation failed: ' + $decoder.Error) }
        if ($decoder.Blocks -gt $lastBlocks) {
            if ($lineCount % 10 -eq 0) {
                Show-Line '  time    block   records   samples   RF seq   sensor(s)    raw latest [12 values]' Cyan
            }
            $take = [Math]::Min(12,$decoder.Raw.Length)
            $raw = $decoder.Raw[($decoder.Raw.Length-$take)..($decoder.Raw.Length-1)] -join ','
            Show-Line ('{0,7:0.0}s {1,7} {2,9} {3,9} {4,8} {5,11:0.000}    {6}' -f `
                $clock.Elapsed.TotalSeconds,$decoder.LastBlock,$decoder.Records,$decoder.Samples,`
                $decoder.RadioSequence,($decoder.SensorMs/1000.0),$raw) Green
            Show-Line ('          CRC=OK  USB-gap={0}  record-gap={1}  RF-gap={2}' -f `
                $decoder.UsbGaps,$decoder.TransportGaps,$decoder.RadioGaps) DarkGray
            $lastBlocks=$decoder.Blocks
            $lastPrint=$clock.Elapsed.TotalSeconds
            $lineCount++
            $lastStatus='LIVE'
        } elseif ($clock.Elapsed.TotalSeconds-$lastPrint -ge 5) {
            Show-Line ('{0,7:0.0}s {1}: USB open; waiting for next complete validated block...' -f `
                $clock.Elapsed.TotalSeconds,$lastStatus) Yellow
            $lastPrint=$clock.Elapsed.TotalSeconds
        }
    }
} catch {
    Show-Line ('ERROR: ' + $_.Exception.Message) Red
    $exitCode=1
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
    Show-Line ('END blocks={0} records={1} samples={2} USB-gap={3} record-gap={4} RF-gap={5}' -f `
        $decoder.Blocks,$decoder.Records,$decoder.Samples,$decoder.UsbGaps,$decoder.TransportGaps,$decoder.RadioGaps) Cyan
    if ($saved) { $saved.Dispose() }
}
if ($Seconds -gt 0 -and $decoder.Blocks -eq 0) { $exitCode=1 }
exit $exitCode
