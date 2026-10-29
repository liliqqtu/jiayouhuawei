# -*- coding: utf-8 -*-
"""设备模拟器启动入口。

    python run_sim.py --scenario night --base 02:10
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from sim.device_sim import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
