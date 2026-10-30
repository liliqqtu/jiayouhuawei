/**
 * 我的：网关地址、连接自检、口径说明。
 * 现场换 WiFi / 换电脑时，只改这一页即可，不用重新编译。
 */
import { BusinessError } from '@kit.BasicServicesKit';
import { Theme } from '../common/Theme';
import { ApiClient, DEFAULT_BASE, Settings } from '../service/ApiClient';
import { DeviceAddResult, DeviceStatus } from '../model/Models';
import { FamilyStore } from '../service/FamilyStore';

@Component
export struct MineTab {
  /** 关怀模式（大字）开关，由「我的」页写入 AppStorage */
  @StorageLink('largeFont') largeFont: boolean = false;
  /** 老人极简首页：超大字号，只留状态和三颗大按钮 */
  @StorageLink('elderHome') elderHome: boolean = false;

  @Prop @Watch('onTick') tick: number = 0;
  @State baseUrl: string = Settings.baseUrl;
  /** 用户是否已经改过输入框：改过就不再被自动同步覆盖 */
  @State edited: boolean = false;
  /** 0 标准 / 1 炎热潮湿 / 2 干冷 */
  @State climate: number = 0;
  @State tempHighText: string = '';
  @State humHighText: string = '';
  @State noMotionText: string = '';
  @State napOn: boolean = true;
  @State personalTip: string = '';
  @State status: string = '';
  @State testing: boolean = false;
  /** 待补发操作条数（由 Index 的补发定时器写入 AppStorage） */
  @StorageLink('pendingOps') pendingOps: number = 0;
  /** 当前选中设备（多老人时切换） */
  @StorageLink('deviceId') curDevice: string = '';
  /**
   * 全局刷新信号：切换设备时 +1，首页/告警页 @Watch 到就重新取数。
   * 为什么不直接用 deviceId 的变化：deviceId 变化只覆盖“换设备”，
   * 手动点“刷新”或登记新设备后需要一次“同设备也要重取”的信号。
   */
  @StorageLink('dataVersion') dataVersion: number = 0;
  /** 家庭组：名称与身份由 FamilyStore 写入 AppStorage，加入/退出后这里立即刷新 */
  @StorageLink('familyName') familyName: string = '';
  @StorageLink('familyRole') familyRole: string = '';
  @State devices: DeviceStatus[] = [];
  /** 手动登记设备用：填设备编号（如 bench-09）后点添加 */
  @State newDeviceId: string = '';
  /** 绿字：操作成功的短提示（如“已切换到 xxx”），3 秒后自动消失 */
  @State deviceTip: string = '';
  /** 红字：失败原因常驻到下一次成功为止（比 Toast 更容易被家属看到） */
  @State deviceWarn: string = '';
  /** 这一轮设备列表是否取到（网关离线时列表是上一次的缓存，需要标注） */
  @State devicesOnline: boolean = true;
  /** 家属电话输入框：一键呼叫家属拨打它 */
  @State phoneText: string = Settings.familyPhone;
  @State phoneTip: string = '';
  /** 是否正停在「我的」页：只有可见时才轮询，切走就停，避免后台白耗电 */
  @Prop @Watch('onActiveChange') active: boolean = false;
  /** 10 秒轮询设备状态的定时器（-1 表示没开） */
  private pollTimer: number = -1;
  /** “已切换到 xxx”的 3 秒消失定时器 */
  private tipTimer: number = -1;

  aboutToAppear(): void {
    // 等 Settings.init 完成后同步一次当前地址
    this.baseUrl = Settings.baseUrl;
    // 首次进入就拉一次设备状态；只有当前停在“我的”页才开轮询
    if (this.active) {
      this.startPolling();
    }
  }

  /**
   * 组件销毁时一定要清定时器。
   * 说明：onPageHide 只对 @Entry 页面生效，「我的」是 Tabs 里的子组件，
   * 所以用「父页面把 active 传进来 + aboutToDisappear 兜底」这一对来管生命周期。
   */
  aboutToDisappear(): void {
    this.stopPolling();
    if (this.tipTimer !== -1) {
      clearTimeout(this.tipTimer);
      this.tipTimer = -1;
    }
  }

  /** Tab 切到/切走「我的」页时启停轮询 */
  onActiveChange(): void {
    if (this.active) {
      this.startPolling();
    } else {
      this.stopPolling();
    }
  }

  private startPolling(): void {
    this.stopPolling();
    this.loadDevices();
    this.pollTimer = setInterval(() => {
      this.loadDevices();
    }, 10000);
  }

  private stopPolling(): void {
    if (this.pollTimer !== -1) {
      clearInterval(this.pollTimer);
      this.pollTimer = -1;
    }
  }

  /** Toast 统一入口：提示语短、不改页面状态，适合“点一下就知道结果”的反馈 */
  private toast(msg: string): void {
    try {
      this.getUIContext().getPromptAction().showToast({ message: msg, duration: 2200 });
    } catch (err) {
      const e = err as BusinessError;
      console.error('[mine] toast 失败：' + e.message);
    }
  }

  /** 绿字短提示：显示 msg，3 秒后清掉 */
  private showGreen(msg: string): void {
    this.deviceTip = msg;
    this.deviceWarn = '';
    if (this.tipTimer !== -1) {
      clearTimeout(this.tipTimer);
    }
    this.tipTimer = setTimeout(() => {
      this.deviceTip = '';
      this.tipTimer = -1;
    }, 3000);
  }

  /** 家属电话：只留数字与 +，3~20 位；保存后一键呼叫立即生效 */
  async savePhone(): Promise<void> {
    const digit: string = this.phoneText.replace(/[^0-9+]/g, '');
    if (digit.length < 3 || digit.length > 20) {
      this.phoneTip = '号码格式不对，请输入 3~20 位数字';
      return;
    }
    this.phoneText = digit;
    await Settings.setFamilyPhone(digit);
    this.phoneTip = '已保存，一键呼叫将拨打 ' + digit;
  }

  /** 只下发用户实际填了的项；留空就沿用默认值 */
  async savePersonal(): Promise<void> {
    const parts: string[] = [];
    if (this.tempHighText.length > 0) {
      parts.push('"temp_high":' + this.tempHighText);
    }
    if (this.humHighText.length > 0) {
      parts.push('"hum_high":' + this.humHighText);
    }
    if (this.noMotionText.length > 0) {
      parts.push('"no_motion_min":' + this.noMotionText);
    }
    if (parts.length === 0) {
      this.personalTip = '没有填写内容，沿用默认阈值';
      return;
    }
    const body: string = '{' + parts.join(',') + '}';
    const ok: boolean = await ApiClient.applyConfig(body);
    this.personalTip = ok ? '已保存，按你的设置判定' : '保存失败：网关不可达';
  }

  async applyNap(on: boolean): Promise<void> {
    const body: string = on ? '{nap_enabled:1}' : '{nap_enabled:0}';
    const ok: boolean = await ApiClient.applyConfig(body);
    this.personalTip = ok
      ? (on ? '午睡时段已开启（13:00-15:00 不判无活动）' : '午睡时段已关闭')
      : '设置失败：网关不可达';
    if (ok) {
      this.napOn = on;
    }
  }

  /** 设备切换：写持久化 + AppStorage，首页/告警页会立即重取 */
  /** 设备副标题：有待确认就先说有几条，再给最后上报时间 —— 多设备时判断"谁出事了"靠这一行 */
  deviceSubText(d: DeviceStatus): string {
    const n: number = d.pendingAlarms === undefined ? 0 : d.pendingAlarms;
    const time: string = d.lastSeen.length >= 16 ? d.lastSeen.substring(11, 16) : d.lastSeen;
    if (n > 0) {
      return '待确认 ' + n.toString() + ' 条 · 最后上报 ' + time;
    }
    return '最后上报 ' + time;
  }

  deviceSubColor(d: DeviceStatus): string {
    const n: number = d.pendingAlarms === undefined ? 0 : d.pendingAlarms;
    if (n > 0) {
      return Theme.red;
    }
    return d.online ? Theme.green : Theme.amber;
  }

  /** 最后上报时间的短写法：当天只给 HH:mm，太旧的给 MM-dd HH:mm，避免一长串 ISO 串 */
  lastSeenText(d: DeviceStatus): string {
    const raw: string = d.lastSeen;
    if (raw === null || raw === undefined || raw.length < 16) {
      return '还没有上报';
    }
    return '最后上报 ' + raw.substring(5, 16).replace('T', ' ');
  }

  /** source 标签文案：真实设备 / 模拟演示 / 未上报（数据诚信：不让模拟数据冒充真实采集） */
  sourceText(d: DeviceStatus): string {
    const src: string = d.source === undefined || d.source === null ? '' : (d.source as string);
    if (src === 'hardware') {
      return '真实设备';
    }
    if (src === 'test') {
      return '模拟演示';
    }
    return '未上报';
  }

  sourceColor(d: DeviceStatus): string {
    const src: string = d.source === undefined || d.source === null ? '' : (d.source as string);
    if (src === 'hardware') {
      return Theme.blue;
    }
    if (src === 'test') {
      return Theme.orange;
    }
    return Theme.muted;
  }

  /**
   * 切换设备：写 Preferences + AppStorage，再让 dataVersion 递增触发全局刷新。
   *
   * 两层通知的原因：
   * - Settings.setDevice 里的 AppStorage('deviceId') 让正在显示的页知道“换人了”；
   * - dataVersion 多给一个“即使 ID 没变也重取一次”的口子（登记新设备后要用）。
   */
  async switchDevice(deviceId: string): Promise<void> {
    if (deviceId === this.curDevice) {
      return;
    }
    try {
      await Settings.setDevice(deviceId);
      AppStorage.setOrCreate<number>('dataVersion', this.dataVersion + 1);
      this.showGreen('已切换到 ' + deviceId);
      this.loadDevices();
    } catch (err) {
      const e = err as BusinessError;
      console.error('[device] 切换失败：' + e.message);
      this.deviceWarn = '切换失败：本地保存异常，请重试';
      this.toast('切换失败，请重试');
    }
  }

  /** 登记一台还没上报过的设备，成功后立刻刷新列表 */
  async addDevice(): Promise<void> {
    const id: string = this.newDeviceId.trim();
    if (id.length === 0) {
      this.deviceTip = '请先填设备编号，例如 bench-09';
      return;
    }
    const res: DeviceAddResult = await ApiClient.registerDevice(id);
    if (res.kind === 'offline') {
      // 网络不通：Toast + 常驻红字，两条路径都给，老花眼也能看见
      this.toast('无法连接网关，请先保存并测试连接');
      this.deviceWarn = '无法连接网关，请先保存并测试连接';
      return;
    }
    if (res.kind !== 'ok') {
      this.toast(res.message);
      this.deviceWarn = res.message;
      return;
    }
    this.newDeviceId = '';
    this.showGreen('已登记 ' + id + '，等它上报第一帧数据就会变在线');
    this.loadDevices();
  }

  async loadDevices(): Promise<void> {
    const list: DeviceStatus[] | null = await ApiClient.devices();
    if (list === null) {
      // 保留上一次的列表（比清空更友好：家属还能看到“上次看到的设备”），但明确标注已过期
      this.devicesOnline = false;
      this.deviceWarn = '网关离线：下面的是上次取到的列表，状态可能已过期';
      return;
    }
    this.devicesOnline = true;
    if (this.deviceWarn.indexOf('网关离线') === 0) {
      this.deviceWarn = '';
    }
    this.devices = list;
  }

  onTick(): void {
    // Settings.init 是异步的，组件可能先挂载。这里等它就绪后再补一次同步，
    // 但用户一旦开始编辑就不覆盖，避免边打字边被改。
    if (!this.edited && this.baseUrl !== Settings.baseUrl) {
      this.baseUrl = Settings.baseUrl;
    }
    // 设备状态交给 10 秒轮询，这里不再顺手拉，避免两套节奏打架
  }

  async saveAndTest(): Promise<void> {
    this.testing = true;
    this.status = '正在连接…';
    await Settings.save(this.baseUrl.trim());
    const ok: boolean = await ApiClient.health();
    this.testing = false;
    this.status = ok ? '连接成功，可以开始演示' : '连接失败：检查网关是否用 --lan 启动、手机与电脑是否同一 WiFi';
  }

  /** 家庭组入口：组名 + 我的身份，点进去看成员和邀请 */
  @Builder
  familyCard() {
    Column() {
      Row() {
        Text('家庭组')
          .fontSize(Theme.fs(15, this.largeFont))
          .fontWeight(FontWeight.Medium)
          .fontColor(Theme.navy)
        Blank()
        Text('›')
          .fontSize(Theme.fs(18, this.largeFont))
          .fontColor(Theme.muted)
      }
      .width('100%')
      Text(this.familyName.length > 0
        ? (this.familyName + ' · ' + (this.familyRole === 'owner' ? '户主' : '监护人'))
        : '还没加入家庭组，点这里创建或加入')
        .fontSize(Theme.fs(14, this.largeFont))
        .fontColor(this.familyName.length > 0 ? Theme.slate : Theme.orange)
        .width('100%')
        .margin({ top: 8 })
      Text('成员共享同一份监测数据：谁在家、谁出门、有没有异常，一家人看到的是同一份。')
        .fontSize(Theme.fs(12, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
        .margin({ top: 6 })
        .lineHeight(Theme.fs(18, this.largeFont))
    }
    .width('100%')
    .padding(Theme.page)
    .backgroundColor(Theme.card)
    .borderRadius(Theme.radius)
    .onClick(() => {
      try {
        this.getUIContext().getRouter().pushUrl({ url: 'pages/FamilyPage' });
      } catch (err) {
        const e = err as BusinessError;
        this.deviceTip = '打开家庭组失败：' + e.message;
      }
    })
  }

  @Builder
  infoRow(label: string, value: string) {
    Row() {
      Text(label).fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted)
      Blank()
      Text(value).fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.slate)
    }
    .width('100%')
    .padding({ top: 8, bottom: 8 })
  }

  build() {
    Column() {
      Row() {
        Text('我的').fontSize(Theme.fs(20, this.largeFont)).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
      }
      .width('100%')
      .padding(Theme.page)
      .backgroundColor(Theme.navy)

      Scroll() {
        Column({ space: Theme.gap }) {
          this.familyCard()
          // 网关设置
          Column() {
            Text('网关地址').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted).width('100%')
            TextInput({ text: this.baseUrl, placeholder: DEFAULT_BASE })
              .fontSize(Theme.fs(14, this.largeFont))
              .height(44)
              .margin({ top: 8 })
              .onChange((value: string) => {
                this.baseUrl = value;
                this.edited = true;
              })
            Text('格式：http://电脑局域网IP:8780（网关启动时会打印）')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 6 })

            Button(this.testing ? '连接中…' : '保存并测试连接')
              .width('100%')
              .height(44)
              .fontSize(Theme.fs(15, this.largeFont))
              .backgroundColor(Theme.blue)
              .fontColor('#FFFFFF')
              .borderRadius(12)
              .margin({ top: 12 })
              .onClick(() => {
                this.saveAndTest();
              })

            if (this.status.length > 0) {
              Text(this.status)
                .fontSize(Theme.fs(12, this.largeFont))
                .fontColor(this.status.indexOf('成功') >= 0 ? Theme.green : Theme.orange)
                .width('100%')
                .margin({ top: 8 })
            }
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 设备切换（多老人 / 多台设备）
          Column() {
            Row() {
              Text('设备切换')
                .fontSize(Theme.fs(15, this.largeFont))
                .fontWeight(FontWeight.Medium)
                .fontColor(Theme.navy)
              Blank()
              Text(this.devices.length.toString() + ' 台')
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(this.largeFont ? Theme.navy : Theme.muted)
            }
            .width('100%')

            Text(this.curDevice.length > 0 ? '当前：' + this.curDevice : '当前：未指定（显示最近上报的设备）')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(this.largeFont ? Theme.navy : Theme.muted)
              .width('100%')
              .margin({ top: 4 })
            // 成功提示（绿，3 秒后自动消失）与失败原因（红，常驻到下次成功）放在“当前”下面，
            // 不用滚动卡片就能看到反馈——老人未必知道要往下滑。
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
              Text(this.devicesOnline
                ? '还没有设备：板子第一次上报会自动出现在这里，也可以在下面登记一台。'
                : '网关离线时取不到设备列表，仍按上次选择显示')
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(this.largeFont ? Theme.navy : Theme.muted)
                .width('100%')
                .margin({ top: 8 })
            } else {
              ForEach(this.devices, (d: DeviceStatus) => {
                Column() {
                  Row() {
                    Row() {
                      Text(d.deviceId)
                        .fontSize(Theme.fs(14, this.largeFont))
                        .fontColor(Theme.slate)
                      // 数据来源标签：真实设备 / 模拟演示 / 未上报（数据诚信，不允许含糊）
                      Text(this.sourceText(d))
                        .fontSize(Theme.fs(10, this.largeFont))
                        .fontColor(this.sourceColor(d))
                        .padding({ left: 6, right: 6, top: 2, bottom: 2 })
                        .backgroundColor(this.sourceText(d) === '真实设备' ? '#EFF6FF'
                          : (this.sourceText(d) === '模拟演示' ? '#FFF7ED' : '#F1F5F9'))
                        .borderRadius(6)
                        .margin({ left: 6 })
                    }
                    Blank()
                    if (d.deviceId === this.curDevice) {
                      Text('当前')
                        .fontSize(Theme.fs(12, this.largeFont))
                        .fontColor(Theme.muted)
                        .padding({ left: 12, right: 12, top: 5, bottom: 5 })
                        .backgroundColor('#F1F5F9')
                        .borderRadius(10)
                    } else {
                      Button('切换')
                        .fontSize(Theme.fs(12, this.largeFont))
                        .fontColor('#FFFFFF')
                        .height(32)
                        .padding({ left: 14, right: 14 })
                        .backgroundColor(Theme.blue)
                        .borderRadius(10)
                        .onClick(() => {
                          this.switchDevice(d.deviceId);
                        })
                    }
                  }
                  .width('100%')

                  // 状态行：在线/离线用“文字 + 颜色”两条线表达，色弱人群只靠颜色会分不清
                  Row() {
                    Text(d.online ? '● 在线' : '○ 离线')
                      .fontSize(Theme.fs(11, this.largeFont))
                      .fontColor(d.online ? Theme.green : Theme.amber)
                      .fontWeight(d.online ? FontWeight.Normal : FontWeight.Medium)
                    Text(' · 电量 ' + (d.battery === null ? '—' : Math.round(d.battery).toString() + '%'))
                      .fontSize(Theme.fs(11, this.largeFont))
                      .fontColor(this.largeFont ? Theme.navy : Theme.muted)
                    Text(' · ' + this.lastSeenText(d))
                      .fontSize(Theme.fs(11, this.largeFont))
                      .fontColor(this.deviceSubColor(d))
                  }
                  .width('100%')
                  .margin({ top: 3 })

                  // 有待确认告警时单独一行说清楚，这是多设备场景里“谁出事了”的关键
                  if (d.pendingAlarms !== undefined && d.pendingAlarms > 0) {
                    Text('待确认告警 ' + d.pendingAlarms.toString() + ' 条，可在「告警」页处理')
                      .fontSize(Theme.fs(11, this.largeFont))
                      .fontColor(Theme.red)
                      .width('100%')
                      .margin({ top: 3 })
                  }

                  // 离线设备的额外说明：给“为什么点了切换没数据”一个解释，避免家属以为坏了
                  if (!d.online) {
                    Text('这台设备现在没有数据上报，切过去后首页会显示“数据已过期”。')
                      .fontSize(Theme.fs(10, this.largeFont))
                      .fontColor(this.largeFont ? Theme.slate : Theme.muted)
                      .width('100%')
                      .margin({ top: 3 })
                  }
                }
                .width('100%')
                .alignItems(HorizontalAlign.Start)
                .padding({ top: 8, bottom: 8 })
              }, (d: DeviceStatus) => d.deviceId)
            }

            // 手动加入其他设备：真板子靠"第一帧自动建档"，这里给没上报过的设备占位
            Row({ space: 8 }) {
              TextInput({ text: this.newDeviceId, placeholder: '设备编号，如 bench-09' })
                .layoutWeight(1)
                .height(40)
                .fontSize(Theme.fs(13, this.largeFont))
                .backgroundColor('#F1F5F9')
                .borderRadius(10)
                .onChange((v: string) => {
                  this.newDeviceId = v;
                })
              Button('添加设备')
                .fontSize(Theme.fs(13, this.largeFont))
                .fontColor('#FFFFFF')
                .height(40)
                .padding({ left: 14, right: 14 })
                .backgroundColor(Theme.navy)
                .borderRadius(10)
                .onClick(() => {
                  this.addDevice();
                })
            }
            .width('100%')
            .margin({ top: 12 })

            Text('说明：设备第一次上报数据会自动出现在这里；还没上报的，可以先填编号登记占位（只是登记，不会生成假数据）。')
              .fontSize(Theme.fs(10, this.largeFont))
              .fontColor(this.largeFont ? Theme.slate : Theme.muted)
              .width('100%')
              .margin({ top: 6 })

            // 失败原因常驻（红），成功提示 3 秒后自动消失（绿）
            if (this.deviceTip.length > 0) {
              Text(this.deviceTip)
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.green)
                .width('100%')
                .margin({ top: 8 })
            }
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })
          // 家属电话：一键呼叫家属拨打的号码
          Column() {
            Text('家属电话')
              .fontSize(Theme.fs(15, this.largeFont))
              .fontWeight(FontWeight.Medium)
              .fontColor(Theme.navy)
            Text('告警详情页「一键呼叫家属」拨打这个号码；真机直接拉起拨号盘，模拟器会把号码复制出来')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .margin({ top: 2 })
            Row() {
              TextInput({ text: this.phoneText, placeholder: '如 13800138000' })
                .type(InputType.PhoneNumber)
                .layoutWeight(1)
                .height(44)
                .fontSize(Theme.fs(14, this.largeFont))
                .backgroundColor('#F1F4F8')
                .borderRadius(10)
                .onChange((v: string) => {
                  this.phoneText = v;
                })
              Button('保存')
                .height(44)
                .width(84)
                .margin({ left: 10 })
                .backgroundColor(Theme.navy)
                .fontColor('#FFFFFF')
                .fontSize(Theme.fs(14, this.largeFont))
                .onClick(() => {
                  this.savePhone();
                })
            }
            .width('100%')
            .margin({ top: 10 })

            if (this.phoneTip.length > 0) {
              Text(this.phoneTip)
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(this.phoneTip.indexOf('已') === 0 ? Theme.green : Theme.red)
                .margin({ top: 8 })
            }
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 关怀模式（大字）
          Column() {
            Row() {
              Column() {
                Text('关怀模式（大字）')
                  .fontSize(Theme.fs(15, this.largeFont))
                  .fontWeight(FontWeight.Medium)
                  .fontColor(Theme.navy)
                Text('字号整体放大，老人和家属都看得清')
                  .fontSize(Theme.fs(11, this.largeFont))
                  .fontColor(Theme.muted)
                  .margin({ top: 2 })
              }
              .alignItems(HorizontalAlign.Start)
              .layoutWeight(1)

              Toggle({ type: ToggleType.Switch, isOn: this.largeFont })
                .selectedColor(Theme.navy)
                .onChange((on: boolean) => {
                  Settings.setLargeFont(on);
                })
            }
            .width('100%')

            if (this.pendingOps > 0) {
              Text('待补发操作 ' + this.pendingOps + ' 条：断网期间的确认/处置已暂存，联网后自动提交')
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.amber)
                .width('100%')
                .margin({ top: 8 })
            }
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 前台提醒说明（不做开关：报警类应用不能让人误关掉自己的提醒）
          Column() {
            Row() {
              Text('本机提醒')
                .fontSize(Theme.fs(15, this.largeFont))
                .fontWeight(FontWeight.Medium)
                .fontColor(Theme.navy)
                .layoutWeight(1)
              Text('始终开启')
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.green)
                .backgroundColor('#E8F7EE')
                .padding({ left: 8, right: 8, top: 2, bottom: 2 })
                .borderRadius(6)
            }
            .width('100%')

            Text('APP 停留在前台时，出现新的待确认告警会自动震动并播放提示音（同一等级 15 秒内只响一次）。'
              + '这是安全提醒功能，故意不提供总开关，避免误关后漏掉告警。')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 6 })

            Text('想彻底不吵：关掉 APP 或切到后台即可（后台不提醒，也说明它确实不是系统推送）。')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 4 })
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 老人极简首页（给老人自己拿着手机看）
          Column() {
            Row() {
              Column() {
                Text('老人极简首页')
                  .fontSize(Theme.fs(15, this.largeFont))
                  .fontWeight(FontWeight.Medium)
                  .fontColor(Theme.navy)
                Text('超大字号，首页只留“是否正常 + 一键求助 + 两个大按钮”，适合视力不好的老人自己看')
                  .fontSize(Theme.fs(11, this.largeFont))
                  .fontColor(Theme.muted)
                  .margin({ top: 2 })
              }
              .alignItems(HorizontalAlign.Start)
              .layoutWeight(1)

              Toggle({ type: ToggleType.Switch, isOn: this.elderHome })
                .selectedColor(Theme.navy)
                .onChange((on: boolean) => {
                  this.elderHome = on;
                })
            }
            .width('100%')
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 个性化设置
          Column() {
            Text('个性化设置').fontSize(Theme.fs(15, this.largeFont)).fontWeight(FontWeight.Medium)
              .fontColor(Theme.navy).width('100%')
            Text('按自家情况填，留空则用默认值')
              .fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 }).width('100%')

            Row() {
              Text('温度上限').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.slate).width(78)
              TextInput({ text: this.tempHighText, placeholder: '默认 35' })
                .type(InputType.Number).height(38).layoutWeight(1)
                .onChange((v: string) => { this.tempHighText = v; })
              Text('℃').fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.muted).width(22)
            }.width('100%').margin({ top: 10 })

            Row() {
              Text('湿度上限').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.slate).width(78)
              TextInput({ text: this.humHighText, placeholder: '默认 80' })
                .type(InputType.Number).height(38).layoutWeight(1)
                .onChange((v: string) => { this.humHighText = v; })
              Text('%').fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.muted).width(22)
            }.width('100%').margin({ top: 8 })

            Row() {
              Text('无活动判定').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.slate).width(78)
              TextInput({ text: this.noMotionText, placeholder: '默认 5' })
                .type(InputType.Number).height(38).layoutWeight(1)
                .onChange((v: string) => { this.noMotionText = v; })
              Text('分钟').fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.muted).width(30)
            }.width('100%').margin({ top: 8 })

            Button('保存阈值')
              .width('100%').height(38).fontSize(Theme.fs(14, this.largeFont)).margin({ top: 10 })
              .backgroundColor(Theme.navy).fontColor(Color.White)
              .onClick(() => { this.savePersonal(); })

            Row() {
              Column() {
                Text('午睡时段').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.slate)
                Text('13:00-15:00 不判“无活动”').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted)
              }.alignItems(HorizontalAlign.Start).layoutWeight(1)

              Toggle({ type: ToggleType.Switch, isOn: this.napOn })
                .selectedColor(Theme.navy)
                .onChange((on: boolean) => { this.applyNap(on); })
            }
            .width('100%').margin({ top: 12 })

            if (this.personalTip.length > 0) {
              Text(this.personalTip).fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.green)
                .width('100%').margin({ top: 8 })
            }
          }
          .width('100%').padding(16).backgroundColor(Theme.card)
          .borderRadius(Theme.radius).margin({ top: 12 })

          // 数据来源说明
          Column() {
            Text('数据来源标识').fontSize(Theme.fs(13, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.navy).width('100%')
            this.infoRow('真实采集', '来自开发板与传感器')
            this.infoRow('按键模拟', '板载按键 / 模拟器替代')
            this.infoRow('测试数据', '回归测试专用')
            Text('凡是模拟数据，首页与告警卡片都会显式标注，不冒充真实采集。')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 6 })
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 判定口径
          Column() {
            Text('判定口径（演示参数）').fontSize(Theme.fs(13, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.navy).width('100%')
            this.infoRow('L1 提醒', '温湿度越界 / 门窗开启超时')
            this.infoRow('L2 关注', '白天连续无活动')
            this.infoRow('L3 紧急', '夜间卫生间无活动 / 求助按钮')
            Text('系统只判断“有没有活动”，不判断“在做什么”，也不对姿态下结论。')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 6 })
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 关于
          Column() {
            Text('守夜灯 · 独居老人居家安全监测与应急辅助系统')
              .fontSize(Theme.fs(12, this.largeFont))
              .fontColor(Theme.slate)
              .width('100%')
            Text('OpenHarmony 独居老人居家安全守护 · v1.0.0')
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 4 })
          }
          .width('100%')
          .padding(14)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })
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
