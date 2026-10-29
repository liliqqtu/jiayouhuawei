# -*- coding: utf-8 -*-
"""临时探针：验证多设备链路（/api/v1/devices、?deviceId=、未知设备 404）。

起一个临时网关（8781 + 独立 db），用两台设备各报一帧，检查看板/快照/告警
是否按设备隔离，跑完自动关掉网关、删临时库。
"""
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timedelta, timezone

TZ = timezone(timedelta(hours=8))

S = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PY = sys.executable
PORT = 8781
BASE = 'http://127.0.0.1:%d' % PORT
DB = os.path.join(S, 'data', '_probe_multi.db')


def cleanup():
    for suffix in ('', '-wal', '-shm'):
        p = DB + suffix
        if os.path.exists(p):
            os.remove(p)


def req(path, body=None, method=None):
    data = None if body is None else json.dumps(body).encode('utf-8')
    r = urllib.request.Request(BASE + path, data=data, method=method)
    if data is not None:
        r.add_header('Content-Type', 'application/json')
    with urllib.request.urlopen(r, timeout=5) as resp:
        return resp.getcode(), json.loads(resp.read().decode('utf-8'))


def frame(device_id, motion):
    now = datetime.now(TZ).replace(microsecond=0).isoformat()
    return {
        'deviceId': device_id, 'seq': 1, 'ts': now,
        'pirDetected': motion, 'lastMotionTs': now if motion else None,
        'lastMotionZone': 'living' if motion else None,
        'temperature': 23.4, 'humidity': 52.0, 'lux': 180.0,
        'doorOpen': False, 'sosPressed': False, 'gas': None, 'smoke': None,
        'powerState': 'mains', 'battery': 95.0, 'rssi': -55,
        'online': True, 'fwVersion': '1.0.0', 'source': 'test',
    }


def main():
    cleanup()
    gw = subprocess.Popen(
        [PY, os.path.join(S, 'run_gateway.py'), '--port', str(PORT), '--db', DB],
        cwd=S, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    try:
        for _ in range(40):
            try:
                req('/health')
                break
            except Exception:
                time.sleep(0.25)
        else:
            raise SystemExit('网关没起来')

        print('1) 上报 elder-001 有活动 / elder-002 无活动')
        print('   ', req('/api/v1/snapshot', frame('elder-001', True))[0])
        print('   ', req('/api/v1/snapshot', frame('elder-002', False))[0])

        code, devs = req('/api/v1/devices')
        print('2) GET /api/v1/devices -> %d，count=%s，设备: %s' % (
            code, devs['count'], [d['deviceId'] for d in devs['items']]))

        code, snap1 = req('/api/v1/snapshot?deviceId=elder-001')
        code2, snap2 = req('/api/v1/snapshot?deviceId=elder-002')
        print('3) 快照按设备隔离: elder-001.motion=%s / elder-002.motion=%s' % (
            snap1['pirDetected'], snap2['pirDetected']))

        try:
            req('/api/v1/snapshot?deviceId=elder-999')
            print('4) ❌ 未知设备未 404')
        except urllib.error.HTTPError as e:
            print('4) 未知设备 -> %d（预期 404）' % e.code)

        html = urllib.request.urlopen(BASE + '/', timeout=5).read().decode('utf-8')
        print('5) 看板可访问: / -> 200，含设备下拉: %s' % ('id="devSel"' in html))
    finally:
        gw.terminate()
        try:
            gw.wait(timeout=5)
        except Exception:
            gw.kill()
        time.sleep(0.3)
        cleanup()
        print('临时网关已关闭，临时库已删除')


if __name__ == '__main__':
    main()
