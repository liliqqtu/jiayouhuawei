import { Theme } from '../common/Theme';

/**
 * 首次启动的新手引导：告诉用户怎么用、声音代表什么、遇事怎么办。
 * 关掉后不再显示（Settings.guideDone 记在本地）。
 */
@Component
export struct Guide {
  /** 关怀模式（大字）开关，由「我的」页写入 AppStorage */
  @StorageLink('largeFont') largeFont: boolean = false;

  onDone: () => void = () => {};

  @Builder
  section(title: string, lines: string[]) {
    Column() {
      Text(title).fontSize(Theme.fs(15, this.largeFont)).fontWeight(FontWeight.Medium)
        .fontColor(Theme.navy).width('100%').margin({ bottom: 6 })
      ForEach(lines, (line: string) => {
        Text('· ' + line).fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.slate)
          .width('100%').margin({ bottom: 4 })
      })
    }
    .width('100%').alignItems(HorizontalAlign.Start).margin({ bottom: 16 })
  }

  build() {
    Column() {
      Text('欢迎使用守夜灯').fontSize(Theme.fs(22, this.largeFont)).fontWeight(FontWeight.Bold)
        .fontColor(Theme.navy).margin({ top: 28, bottom: 4 })
      Text('独居老人居家安全监测').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted)
        .margin({ bottom: 20 })

      Scroll() {
        Column() {
          this.section('三步就能用起来', [
            '「我的」页填写网关地址（电脑 IP + :8780），点保存并测试连接',
            '「首页」看五项指标、在线状态和有没有待处理告警',
            '「告警」页处理待确认：先确认，再填处置结果，最后关闭'
          ])

          this.section('听到声音要分得清', [
            'L3 紧急：连续警笛声（响到有人处理），立即打电话或上门',
            'L2 关注：三声提示音，尽快确认',
            'L1 提醒：两声轻提示，方便时处理'
          ])

          this.section('遇到这些情况先做什么', [
            '求助/无活动：先打电话，无人接听立刻找邻居上门',
            '疑似火情/烟雾：先撤离再报警，不要先收拾东西',
            '燃气：开窗、关阀、别开灯，撤到室外再打电话',
            '温度湿度过高过低：先查取暖/除湿设备，再提醒老人',
            '门未关/外出未归：先确认是否正常，超时联系家属或社区'
          ])

          this.section('数据诚实说明', [
            '标“测试/模拟”的数据不是真实传感器采集',
            '没接传感器的项显示“—”，不会编造数值'
          ])
        }
        .width('100%')
      }
      .layoutWeight(1)
      .scrollBar(BarState.Off)

      Button('我知道了，开始使用')
        .width('100%').height(46).fontSize(Theme.fs(16, this.largeFont))
        .backgroundColor(Theme.navy).fontColor(Color.White)
        .margin({ top: 12, bottom: 20 })
        .onClick(() => { this.onDone(); })
    }
    .width('100%').height('100%')
    .padding({ left: 22, right: 22 })
    .backgroundColor(Theme.bg)
  }
}
