/**
 * 首页：三秒内回答“老人现在好不好”。
 * 信息优先级：是否正常 > 最后活动 > 环境 > 求助入口。
 */
import { Theme } from '../common/Theme';
import { MetricCell } from './MetricCell';
import { ApiClient, Settings } from '../service/ApiClient';
import { SensorSnapshot, humanAgo, isHardware, secondsSince, shortTime, sourceLabel, zoneLabel } from '../model/Models';

@Component
export struct HomeTab {
  /** 关怀模式（大字）开关，由「我的」页写入 AppStorage */
  @StorageLink('largeFont') largeFont: boolean = false;
  /** 老人极简首页：只留状态带 + 三个大按钮，字号最大 */
  @StorageLink('elderHome') elderHome: boolean = false;
  /** 由 Index 传进来，用来切页签（极简版点“告警/我的”直接跳） */
  // 函数类型不能用 @Prop/@State 装饰（API 23+ 限制），普通成员即可传值
  onJump: (index: number) => void = (index: number) => {};
  /** 当前选中设备（「我的」页切换后立即重取本页数据） */
  @StorageLink('deviceId') @Watch('onDeviceChange') deviceId: string = '';

  @Prop @Watch('onTick') tick: number = 0;

  @State snap: SensorSnapshot | null = null;
  @State online: boolean = false;
  /** 网关本身是否可达（与“设备是否在线”区分开） */
  @State reachable: boolean = true;
  /** 地址必须挂到 @State 上才会重绘：静态表达式 ArkUI 认为永不变化 */
  @State displayUrl: string = Settings.baseUrl;
  /** 1 秒跳一次，只用于刷新“N 秒前”这类相对时间，不额外请求网络 */
  @State clock: number = 0;
  private clockTimer: number = -1;
  @State loaded: boolean = false;
  @State sosBusy: boolean = false;
  @State toast: string = '';

  aboutToAppear(): void {
    this.load();
    this.clockTimer = setInterval(() => {
      this.clock += 1;
    }, 1000);
  }

  aboutToDisappear(): void {
    if (this.clockTimer !== -1) {
      clearInterval(this.clockTimer);
    }
  }

  /** 温度是否越界（演示阈值：16~30℃） */
  tempBad(): boolean {
    const t: number | null = this.snap === null ? null : this.snap.temperature;
    return t !== null && (t > 30 || t < 16);
  }

  /** 湿度是否越界（30~80%） */
  humBad(): boolean {
    const h: number | null = this.snap === null ? null : this.snap.humidity;
    return h !== null && (h > 80 || h < 30);
  }

  envWarning(): string {
    if (this.tempBad() && this.snap !== null && this.snap.temperature !== null) {
      return this.snap.temperature > 30 ? '环境异常：室内温度偏高' : '环境异常：室内温度偏低';
    }
    if (this.humBad() && this.snap !== null && this.snap.humidity !== null) {
      return this.snap.humidity > 80 ? '环境异常：室内湿度过高' : '环境异常：室内湿度过低';
    }
    return '';
  }

  onTick(): void {
    this.load();
  }

  /** 切换老人/设备后立刻刷新，不等下一个 3 秒心跳 */
  onDeviceChange(): void {
    this.load();
  }

  async load(): Promise<void> {
    const data: SensorSnapshot | null = await ApiClient.snapshot();
    if (data !== null) {
      this.reachable = true;
      this.snap = data;
      this.online = data.online;
    } else {
      // 404（还没数据）和“网关没起”都会返回 null，用 /health 再判一次
      this.snap = null;
      this.online = false;
      this.reachable = await ApiClient.health();
    }
    this.displayUrl = Settings.baseUrl;
    this.loaded = true;
  }

  /** 首页“最后活动”整句：没有区域信息时就不带“· 未知区域” */
  motionText(): string {
    if (this.snap === null) {
      return this.reachable ? '已连接网关，等待设备上报数据' : '请在“我的”页填写网关地址';
    }
    const tick: number = this.clock;   // 依赖 1 秒时钟，触发重算
    const ago: string = humanAgo(secondsSince(this.snap.lastMotionTs));
    if (this.snap.lastMotionTs === null) {
      return '尚未检测到人体活动';
    }
    const time: string = shortTime(this.snap.lastMotionTs);
    const zone: string = zoneLabel(this.snap.lastMotionZone);
    const base: string = `最后活动 ${ago}（${time}）`;
    return zone.length > 0 ? `${base} · ${zone}` : base;
  }

  /** 极简版状态标题：只说一句话，老人不用读长句 */
  elderTitle(): string {
    if (!this.reachable) {
      return '没连上网关';
    }
    const level: string = this.alarmLevel();
    if (level === 'L3') {
      return '紧急！快去看看';
    }
    if (level === 'L2' || level === 'L1') {
      return '有事要处理';
    }
    if (this.snap === null) {
      return '等待数据';
    }
    if (!this.online) {
      return '设备离线';
    }
    return '一切正常';
  }

  /** 极简版状态图标字符（不用图片资源，字重足够大即可辨识） */
  elderIcon(): string {
    const level: string = this.alarmLevel();
    if (level === 'L3') {
      return '!';
    }
    if (level === 'L2' || level === 'L1') {
      return '!';
    }
    return '✓';
  }

  /** 当前最高等级待确认告警，没有则为空串 */
  alarmLevel(): string {
    if (this.snap === null || this.snap.activeAlarm === undefined || this.snap.activeAlarm === null) {
      return '';
    }
    return this.snap.activeAlarm.level;
  }

  alarmText(): string {
    if (this.snap === null || this.snap.activeAlarm === undefined || this.snap.activeAlarm === null) {
      return '';
    }
    return this.snap.activeAlarm.message;
  }

  statusText(): string {
    if (!this.reachable) {
      return '未连接网关';
    }
    if (this.snap === null) {
      // 多设备场景：网关是通的，但这台设备上报过 0 条快照（例如只建过档的旁听设备）
      return '已连接 · 该设备还没有数据';
    }
    // 有告警时优先显示告警，而不是“一切正常”
    const level: string = this.alarmLevel();
    if (level === 'L3') {
      return '紧急告警待处理';
    }
    if (level === 'L2') {
      return '发现异常待确认';
    }
    if (level === 'L1') {
      return '有环境提醒';
    }
    if (!this.online) {
      return '设备离线 · 数据已过期';
    }
    return '一切正常';
  }

  cardBg(): string {
    if (!this.reachable) {
      return Theme.card;
    }
    const level: string = this.alarmLevel();
    if (level === 'L3') {
      return '#FEF2F2';
    }
    if (level === 'L2' || level === 'L1') {
      return '#FFFBEB';
    }
    return this.online ? '#F0FDF4' : '#FFFBEB';
  }

  cardBorder(): string {
    if (!this.reachable) {
      return Theme.line;
    }
    const level: string = this.alarmLevel();
    if (level === 'L3') {
      return Theme.red;
    }
    if (level === 'L2' || level === 'L1') {
      return Theme.orange;
    }
    return this.online ? Theme.green : Theme.orange;
  }

  dotColor(): string {
    const level: string = this.alarmLevel();
    if (level === 'L3') {
      return Theme.red;
    }
    if (level === 'L2' || level === 'L1') {
      return Theme.orange;
    }
    return this.online ? Theme.green : (this.reachable ? Theme.orange : Theme.muted);
  }

  async triggerSos(): Promise<void> {
    if (this.sosBusy) {
      return;
    }
    this.sosBusy = true;
    // 求助发给“当前正在看的这台”：多设备时不会误发到另一台设备上
    const target: string = this.deviceId.length > 0 ? this.deviceId
      : (this.snap === null ? 'bearpi-001' : this.snap.deviceId);
    const ok: boolean = await ApiClient.sendSos(target);
    this.sosBusy = false;
    this.toast = ok ? '已发出求助，家属会收到告警' : '发送失败：网关未连接';
    setTimeout(() => {
      this.toast = '';
    }, 3000);
  }

  /** 老人极简首页：状态带 + 最后活动 + 三个大按钮 */
  @Builder
  elderPage() {
    Column() {
      Row() {
        Text('守夜灯').fontSize(26).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
        Blank()
        Text(this.online ? '在线' : '离线')
          .fontSize(16)
          .fontColor('#FFFFFF')
      }
      .width('100%')
      .padding({ left: 20, right: 20, top: 18, bottom: 18 })
      .backgroundColor(Theme.navy)

      Scroll() {
        Column({ space: 14 }) {
          // 状态带：一个字都不用读太多
          Row() {
            Text(this.elderIcon())
              .fontSize(40)
              .fontWeight(FontWeight.Bold)
              .fontColor('#FFFFFF')
              .width(64)
              .height(64)
              .textAlign(TextAlign.Center)
              .borderRadius(32)
              .backgroundColor(this.dotColor())
            Text(this.elderTitle())
              .fontSize(30)
              .fontWeight(FontWeight.Bold)
              .fontColor(Theme.navy)
              .margin({ left: 16 })
              .layoutWeight(1)
          }
          .width('100%')
          .padding(20)
          .backgroundColor(this.cardBg())
          .borderRadius(Theme.radius)
          .border({ width: 2, color: this.cardBorder() })

          // 最后活动：老人最关心的那句
          Text(this.motionText())
            .fontSize(20)
            .fontColor(Theme.slate)
            .width('100%')
            .padding(18)
            .backgroundColor(Theme.card)
            .borderRadius(Theme.radius)
            .border({ width: 1, color: Theme.line })

          if (this.alarmText().length > 0) {
            Text('⚠ ' + this.alarmText())
              .fontSize(18)
              .fontColor(Theme.red)
              .width('100%')
              .padding(16)
              .backgroundColor('#FEF2F2')
              .borderRadius(Theme.radius)
              .border({ width: 2, color: Theme.red })
          }

          // 三个大按钮
          Text(this.sosBusy ? '正在发送…' : '一键求助')
            .fontSize(32)
            .fontWeight(FontWeight.Bold)
            .fontColor('#FFFFFF')
            .width('100%')
            .height(130)
            .textAlign(TextAlign.Center)
            .backgroundColor(Theme.orange)
            .borderRadius(20)
            .gesture(LongPressGesture({ repeat: false, duration: 1000 })
              .onAction(() => {
                this.triggerSos();
              }))

          Text('长按 1 秒才发出，防误触')
            .fontSize(14)
            .fontColor(Theme.muted)
            .width('100%')
            .textAlign(TextAlign.Center)

          Row({ space: 12 }) {
            Text('看告警')
              .fontSize(26)
              .fontWeight(FontWeight.Bold)
              .fontColor('#FFFFFF')
              .layoutWeight(1)
              .height(110)
              .textAlign(TextAlign.Center)
              .backgroundColor(Theme.navy)
              .borderRadius(20)
              .onClick(() => {
                this.onJump(1);
              })
            Text('我的')
              .fontSize(26)
              .fontWeight(FontWeight.Bold)
              .fontColor(Theme.navy)
              .layoutWeight(1)
              .height(110)
              .textAlign(TextAlign.Center)
              .backgroundColor('#E2E8F0')
              .borderRadius(20)
              .onClick(() => {
                this.onJump(3);
              })
          }
          .width('100%')

          if (this.toast.length > 0) {
            Text(this.toast).fontSize(16).fontColor(Theme.orange).width('100%').textAlign(TextAlign.Center)
          }

          // 诚实说明：测试数据必须标出来
          Text(this.snap === null ? '数据来源 —'
            : ('数据来源 ' + sourceLabel(this.snap.source) + ' · ' + (isHardware(this.snap.source) ? '真实传感器' : '模拟数据')))
            .fontSize(13)
            .fontColor(isHardware(this.snap === null ? '' : this.snap.source) ? Theme.muted : Theme.orange)
            .width('100%')
            .textAlign(TextAlign.Center)
            .margin({ top: 6 })
        }
        .padding(16)
      }
      .layoutWeight(1)
      .scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Theme.bg)
  }

  build() {
    if (this.elderHome) {
      this.elderPage()
    } else {
      this.normalPage()
    }
  }

  @Builder
  normalPage() {
    Column() {
      // 顶部
      Row() {
        Column() {
          Text('守夜灯').fontSize(Theme.fs(22, this.largeFont)).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
          Text('独居老人居家安全监测').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.blueLight).margin({ top: 2 })
        }
        .alignItems(HorizontalAlign.Start)

        Blank()

        Row() {
          Column()
            .width(8)
            .height(8)
            .borderRadius(4)
            .backgroundColor(this.online ? Theme.green : Theme.muted)
          Text(this.online ? '在线' : '离线')
            .fontSize(Theme.fs(12, this.largeFont))
            .fontColor('#FFFFFF')
            .margin({ left: 6 })
        }
      }
      .width('100%')
      .padding(Theme.page)
      .backgroundColor(Theme.navy)

      Scroll() {
        Column({ space: Theme.gap }) {
          // 主状态卡
          Column() {
            Row() {
              Column()
                .width(14)
                .height(14)
                .borderRadius(7)
                .backgroundColor(this.dotColor())
              Text(this.statusText())
                .fontSize(Theme.fs(24, this.largeFont))
                .fontWeight(FontWeight.Bold)
                .fontColor(Theme.navy)
                .margin({ left: 10 })
            }
            .width('100%')

            if (this.alarmText().length > 0) {
              Text(`⚠ ${this.alarmText()}`)
                .fontSize(Theme.fs(13, this.largeFont))
                .fontColor(Theme.red)
                .width('100%')
                .margin({ top: 6 })
                .maxLines(2)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
            }

            Text(this.motionText())
              .fontSize(Theme.fs(14, this.largeFont))
              .fontColor(Theme.slate)
              .margin({ top: 8 })
              .width('100%')

            Text(this.snap === null
              ? '—'
              : `${this.online ? '在线' : '离线'} · ${this.snap.powerState === null ? '供电未知' : this.snap.powerState} · 电量 ${this.snap.battery === null ? '—' : Math.round(this.snap.battery) + '%'}`)
              .fontSize(Theme.fs(12, this.largeFont))
              .fontColor(Theme.muted)
              .margin({ top: 4 })
              .width('100%')
          }
          .width('100%')
          .padding(16)
          .backgroundColor(this.cardBg())
          .borderRadius(Theme.radius)
          .border({ width: 1, color: this.cardBorder() })

          // 五指标：人体活动 / 温湿度 / 光照 / 门磁
          Row({ space: 10 }) {
            MetricCell({
              title: '人体活动',
              value: this.snap === null ? '—' : (this.snap.pirDetected ? '有活动' : '无活动'),
              hint: 'PIR'
            })
            MetricCell({
              title: '温度',
              value: this.snap !== null && this.snap.temperature !== null
                ? this.snap.temperature.toFixed(1) + '℃' : '—',
              hint: '室内',
              valueColor: this.tempBad() ? Theme.red : Theme.navy
            })
            MetricCell({
              title: '湿度',
              value: this.snap !== null && this.snap.humidity !== null
                ? Math.round(this.snap.humidity) + '%' : '—',
              hint: '相对',
              valueColor: this.humBad() ? Theme.red : Theme.navy
            })
          }
          .width('100%')

          Row({ space: 10 }) {
            MetricCell({
              title: '光照',
              value: this.snap !== null && this.snap.lux !== null
                ? Math.round(this.snap.lux) + 'lx' : '—',
              hint: '环境'
            })
            MetricCell({
              title: '门磁',
              value: this.snap !== null && this.snap.doorOpen !== null
                ? (this.snap.doorOpen ? '已开' : '已关') : '—',
              hint: '入户'
            })
            MetricCell({
              title: '设备',
              value: this.online ? '在线' : (this.reachable ? '离线' : '未连接'),
              hint: '网关'
            })
          }
          .width('100%')

          if (this.envWarning().length > 0) {
            Text(`⚠ ${this.envWarning()}，已生成环境提醒`)
              .fontSize(Theme.fs(12, this.largeFont))
              .fontColor(Theme.red)
              .width('100%')
              .textAlign(TextAlign.Center)
          }

          // 一键求助
          Column() {
            Button(this.sosBusy ? '发送中…' : '一键求助')
              .width('100%')
              .height(64)
              .fontSize(Theme.fs(22, this.largeFont))
              .fontWeight(FontWeight.Bold)
              .fontColor('#FFFFFF')
              .backgroundColor(Theme.orange)
              .borderRadius(18)
              .gesture(
                LongPressGesture({ repeat: false, duration: 1000 })
                  .onAction(() => {
                    this.triggerSos();
                  })
              )

            Text('长按 1 秒触发 · 本地声光 + 语音播报')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .margin({ top: 8 })
          }
          .width('100%')

          if (this.toast.length > 0) {
            Text(this.toast)
              .fontSize(Theme.fs(13, this.largeFont))
              .fontColor(Theme.amber)
              .width('100%')
              .textAlign(TextAlign.Center)
          }

          // 数据来源与链路
          Column({ space: 6 }) {
            Row() {
              Text('数据来源').fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.muted)
              Blank()
              Text(this.snap === null ? '—' : sourceLabel(this.snap.source))
                .fontSize(Theme.fs(12, this.largeFont))
                .fontColor(this.snap !== null && isHardware(this.snap.source) ? Theme.green : Theme.orange)
            }.width('100%')

            Row() {
              Text('更新时间').fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.muted)
              Blank()
              Text(this.snap === null ? '—' : humanAgo(this.snap.staleSeconds))
                .fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.slate)
            }.width('100%')

            if (this.snap !== null && !this.online) {
              Text('数据已过期：设备离线，以下为最后一次上报')
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.orange)
                .width('100%')
            }

            Row() {
              Text('网关').fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.muted)
              Blank()
              Text(this.displayUrl).fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.slate)
            }.width('100%')
          }
          .width('100%')
          .padding(14)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          if (this.snap !== null && !isHardware(this.snap.source)) {
            Text('⚠ 当前为测试/模拟数据，非真实传感器采集')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.orange)
              .width('100%')
              .textAlign(TextAlign.Center)
          }
        }
        .padding(Theme.page)
      }
      .layoutWeight(1)
      .scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Theme.bg)
  }
}
