# -*- coding: utf-8 -*-
"""把“已切换到 xxx”和失败红字挪到“当前：xxx”下面，避免被卡片底部裁掉。"""
import io, os
p = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 "app", "ShadowLamp", "entry", "src", "main", "ets", "view", "MineTab.ets")
s = io.open(p, encoding="utf-8").read()

bottom = """            // 失败原因常驻（红），成功提示 3 秒后自动消失（绿）
            if (this.deviceWarn.length > 0) {
              Text(this.deviceWarn)
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.red)
                .width('100%')
                .margin({ top: 8 })
            }
            if (this.deviceTip.length > 0) {
              Text(this.deviceTip)
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.green)
                .width('100%')
                .margin({ top: 8 })
            }

"""
assert bottom in s
s = s.replace(bottom, "", 1)

anchor = """            if (this.devices.length === 0) {
              Text(this.devicesOnline"""
tip = """            // 成功提示（绿，3 秒后自动消失）与失败原因（红，常驻到下次成功）放在“当前”下面，
            // 这样不用滚动卡片就能看到反馈——老人未必知道要往下滑。
            if (this.deviceTip.length > 0) {
              Text(this.deviceTip)
                .fontSize(Theme.fs(12, this.largeFont))
                .fontColor(Theme.green)
                .width('100%')
                .margin({ top: 6 })
            }
            if (this.deviceWarn.length > 0) {
              Text(this.deviceWarn)
                .fontSize(Theme.fs(12, this.largeFont))
                .fontColor(Theme.red)
                .width('100%')
                .margin({ top: 6 })
            }

            if (this.devices.length === 0) {
              Text(this.devicesOnline"""
assert anchor in s
s = s.replace(anchor, tip, 1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("tip moved")
