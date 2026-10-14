"""Local demo viewer. Every displayed sample comes from validated live USB DATA."""
import argparse
import datetime
import json
import struct
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'tools'))
from vendor_bulk_host import UsbStreamDecoder, USB_TYPE_DATA, ProtocolError, validate_block

lock = threading.Lock()
stop = threading.Event()
state = dict(connected=False, running=False, blocks=0, records=0, bytes=0,
             crc_errors=0, usb_gaps=0, radio_gaps=0, transport_gaps=0, discarded=0, alignment_bytes=0,
             last_block=None, last_transport=None, samples=[], rate=0,
             sensor_timestamp_ms=None, rf_sequence=None,
             last_received=0, elapsed=0, status='等待连接', file='')
worker = None


def receive(port_name):
    started = time.monotonic()
    decoder = UsbStreamDecoder()
    last_packet = last_transport = last_rf = None
    alignment_bytes = None
    recent_bytes = 0
    rate_started = started
    out = ROOT / 'demo_records'
    out.mkdir(exist_ok=True)
    path = out / (datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f') + '.ubr1')
    try:
        with serial.Serial(port_name, 115200, timeout=0.1) as port, path.open('wb') as saved:
            with lock:
                state.update(connected=True, running=True, status='已连接，等待真实数据', file=str(path))
            while not stop.is_set():
                chunk = port.read(max(1, min(port.in_waiting, 16384)))
                try:
                    frames = decoder.feed(chunk) if chunk else []
                    for frame in frames:
                        if alignment_bytes is None:
                            alignment_bytes = decoder.discarded_bytes
                        usb_gap = int(last_packet is not None and frame.packet_sequence != ((last_packet + 1) & 0xffffffff))
                        last_packet = frame.packet_sequence
                        if frame.frame_type != USB_TYPE_DATA:
                            continue
                        info = validate_block(frame.payload)
                        if info['block_sequence'] != frame.message_id:
                            raise ProtocolError('USB message/block sequence mismatch')
                        waveform = []
                        radio_gaps = transport_gaps = 0
                        for index, record in enumerate(info['records']):
                            seq = record['transport_sequence']
                            if last_transport is not None:
                                delta = (seq - last_transport - 1) & 0xffffffff
                                transport_gaps += delta if delta < 0x80000000 else 1
                            last_transport = seq
                            rf_offset = 64 + index * 248 + 40
                            _, rf_seq, _, rf_flags, sensor_time = struct.unpack_from('<HHHHI', frame.payload, rf_offset)
                            if not (rf_flags & 2) or (rf_flags & 1):
                                raise ProtocolError('当前数据不是 MEMS 实物采样')
                            if last_rf is not None:
                                delta = (rf_seq - last_rf - 1) & 0xffff
                                radio_gaps += delta if delta < 0x8000 else 1
                            last_rf = rf_seq
                            count = record['sample_count']
                            waveform.extend(struct.unpack_from('<' + 'h' * count, frame.payload,
                                                               64 + index * 248 + 52))
                        saved.write(frame.wire)
                        saved.flush()
                        recent_bytes += len(frame.payload)
                        with lock:
                            state['blocks'] += 1
                            state['records'] += info['record_count']
                            state['bytes'] += len(frame.payload)
                            state['usb_gaps'] += usb_gap
                            state['radio_gaps'] += radio_gaps
                            state['transport_gaps'] += transport_gaps
                            state.update(last_block=frame.message_id, last_transport=last_transport,
                                         sensor_timestamp_ms=sensor_time, rf_sequence=rf_seq,
                                         samples=waveform[-768:], last_received=time.monotonic(),
                                         status='正在接收真实 nRF 数据')
                except ProtocolError as exc:
                    with lock:
                        state['crc_errors'] += 1
                        state['status'] = '数据校验失败：' + str(exc)
                    break
                now = time.monotonic()
                with lock:
                    state['alignment_bytes'] = alignment_bytes if alignment_bytes is not None else decoder.discarded_bytes
                    state['discarded'] = max(0, decoder.discarded_bytes - (alignment_bytes or 0)) if alignment_bytes is not None else 0
                    state['elapsed'] = now - started
                    if now - rate_started >= 5:
                        state['rate'] = recent_bytes / (now - rate_started)
                        recent_bytes = 0
                        rate_started = now
                    if state['last_received'] and now - state['last_received'] > 10:
                        state['status'] = 'USB已连接，数据暂停'
    except Exception as exc:
        with lock:
            state['status'] = '连接异常：' + str(exc)
    finally:
        with lock:
            state.update(connected=False, running=False)
            if stop.is_set():
                state['status'] = '接收已停止，数据已保存'


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == '/state':
            with lock:
                content = json.dumps(state, ensure_ascii=False).encode('utf-8')
            kind = 'application/json; charset=utf-8'
        elif self.path in ('/', '/index.html'):
            content = (ROOT / 'demo.html').read_bytes()
            kind = 'text/html; charset=utf-8'
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def do_POST(self):
        if self.path != '/stop':
            self.send_error(404)
            return
        stop.set()
        self.send_response(204)
        self.end_headers()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM6')
    parser.add_argument('--http-port', type=int, default=8765)
    args = parser.parse_args()
    worker = threading.Thread(target=receive, args=(args.port,), daemon=True)
    worker.start()
    server = ThreadingHTTPServer(('127.0.0.1', args.http_port), Handler)
    print(f'DEMO_VIEWER http://127.0.0.1:{args.http_port} USB={args.port}', flush=True)
    try:
        server.serve_forever()
    finally:
        stop.set()
        server.server_close()
        worker.join(timeout=2)
