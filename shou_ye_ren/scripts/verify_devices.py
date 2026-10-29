# -*- coding: utf-8 -*-
"""
真机链路验证：多设备接口是否按预期工作（真起一个 HTTP 服务，用 urllib 打）。

覆盖：
  1. POST 两台设备的快照 -> GET /api/v1/devices 返回 2 台，字段齐全；
  2. GET /api/v1/snapshot?deviceId=A 只回 A 的那帧；
  3. 不带 deviceId 时回最近上报的那台（兼容旧行为）；
  4. GET /api/v1/alarms/active?deviceId=A 只算 A 的待确认；
  5. 不存在设备时 snapshot 返回 404 NO_DATA（不静默串台）。
"""
import io
import json
import os
import subprocess
import sys
import time
import urllib.request
import urllib.error

BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
PORT = 8782
DB = os.path.join(BASE, 'data', 'verify_multi.db')


def post(path, body):
    data = json.dumps(body).encode('utf-8')
    req = urllib.request.Request(BASE_URL + path, data=data,
                                 headers={'Content-Type': 'application/json'}, method='POST')
    with urllib.request.urlopen(req, timeout=5) as resp:
        return resp.status, json.loads(resp.read().decode('utf-8'))


def get(path):
    try:
        with urllib.request.urlopen(BASE_URL + path, timeout=5) as resp:
            return resp.status, json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode('utf-8'))


def snap(device_id, seq, temp, motion_ts):
    return {
        "deviceId": device_id, "ts": time.strftime('%Y-%m-%dT%H:%M:%S+08:00'),
        "seq": seq, "pirDetected": True, "lastMotionTs": motion_ts,
        "lastMotionZone": "unknown", "temperature": temp, "humidity": 45.0,
        "lux": 100.0, "doorOpen": False, "battery": 88.0, "powerState": "mains",
        "rssi": -52, "online": True, "source": "test", "fwVersion": "0.1.0",
    }


def main():
    global BASE_URL
    BASE_URL = 'http://127.0.0.1:%d' % PORT
    for suffix in ('', '-wal', '-shm'):
        p = DB + suffix
        if os.path.exists(p):
            os.remove(p)

    env = dict(os.environ)
    proc = subprocess.Popen([sys.executable, os.path.join(BASE, 'run_gateway.py'),
                             '--port', str(PORT), '--db', DB, '--udp-port', '0'],
                            cwd=BASE, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        ok = False
        for _ in range(40):
            try:
                code, _body = get('/health')
                if code == 200:
                    ok = True
                    break
            except Exception:
                pass
            time.sleep(0.25)
        assert ok, '网关没起来'

        now = time.strftime('%Y-%m-%dT%H:%M:%S+08:00')
        post('/api/v1/snapshot', snap('dev-A', 1, 24.5, now))
        time.sleep(0.05)
        post('/api/v1/snapshot', snap('dev-B', 1, 31.0, now))

        code, body = get('/api/v1/devices')
        assert code == 200, body
        assert body['count'] == 2, body
        ids = [d['deviceId'] for d in body['items']]
        assert ids[0] == 'dev-B', ids            # 最近上报的排最前
        assert 'staleSeconds' in body['items'][0] and 'online' in body['items'][0]
        print('[1] /api/v1/devices  ok  count=%d order=%s' % (body['count'], ids))

        code, a = get('/api/v1/snapshot?deviceId=dev-A')
        assert code == 200 and a['deviceId'] == 'dev-A', (code, a.get('deviceId'))
        assert abs(a['temperature'] - 24.5) < 1e-6, a['temperature']
        print('[2] /api/v1/snapshot?deviceId=dev-A  ok  %.1fC' % a['temperature'])

        code, latest = get('/api/v1/snapshot')
        assert latest['deviceId'] == 'dev-B', latest['deviceId']
        print('[3] /api/v1/snapshot（不带参数） ok  回最近上报 %s' % latest['deviceId'])

        code, act = get('/api/v1/alarms/active?deviceId=dev-A')
        assert code == 200 and act['deviceId'] == 'dev-A', (code, act)
        code, act_b = get('/api/v1/alarms/active?deviceId=dev-B')
        assert act_b['deviceId'] == 'dev-B'
        print('[4] /api/v1/alarms/active  ok  A.pending=%d B.pending=%d'
              % (act['pendingCount'], act_b['pendingCount']))

        code, miss = get('/api/v1/snapshot?deviceId=dev-ZZZ')
        assert code == 404 and miss['code'] == 'NO_DATA', (code, miss)
        print('[5] 未知设备 404 NO_DATA（不串台） ok')

        code, lst = get('/api/v1/snapshots?deviceId=dev-A&limit=10')
        assert code == 200 and all(i['deviceId'] == 'dev-A' for i in lst['items']), lst
        print('[6] /api/v1/snapshots?deviceId 过滤 ok  total=%d' % lst['total'])
        print('\n全部 6 项通过')
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            proc.kill()
        for suffix in ('', '-wal', '-shm'):
            p = DB + suffix
            if os.path.exists(p):
                os.remove(p)


if __name__ == '__main__':
    main()
