# -*- coding: utf-8 -*-
"""网关启动入口。

    python run_gateway.py                 # 仅本机访问
    python run_gateway.py --lan           # 允许手机/开发板访问（演示用这个）
    python run_gateway.py --port 9000 --db data/demo.db
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gateway.server import main  # noqa: E402

if __name__ == "__main__":
    main()
