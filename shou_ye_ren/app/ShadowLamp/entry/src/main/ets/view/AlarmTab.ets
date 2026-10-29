/**
 * 告警中心：
 *   列表页 —— 按状态分栏；网关不可达时自动读本地库（离线可看历史）
 *   详情页 —— 查看判定依据、确认、填写处置并关闭
 *
 * 三条产品约束：
 *   1. 离线也能处置，但不是“假装成功”——先写本地待补发队列（Outbox），
 *      联网后按顺序补发，网关以自己库里的状态为准；
 *   2. 处置必须选类型 + 填结果，否则误报率统计不出来；
 *   3. 补发是幂等的：重复确认/关闭网关只返回当前状态，不会写乱台账。
 */
import { common } from '@kit.AbilityKit';
import { Theme } from '../common/Theme';
import { ApiClient, Settings } from '../service/ApiClient';
import { TtsManager } from '../utils/TtsManager';
import { CallUtils } from '../utils/CallUtils';
import { AlarmStore } from '../service/AlarmStore';
import { Outbox } from '../service/Outbox';
import { adviceFor, typeLabel, AlarmEvent, AlarmEvidence, handleTypeLabel, isHardware, levelLabel, shortTime, sourceLabel, statusLabel } from '../model/Models';

const FILTER_VALUES: string[] = ['pending', 'acknowledged', 'closed'];
const FILTER_LABELS: string[] = ['待确认', '处理中', '已关闭'];

/** 时间线上的一个节点 */
interface TimelineNode {
  title: string;
  time: string;
  done: boolean;
}

/** 解析网关存下来的证据 JSON；解析不了就当没有，绝不因为一条脏数据让详情页白屏 */
function parseEvidence(text: string | null | undefined): AlarmEvidence | null {
  if (text === null || text === undefined || text.length === 0) {
    return null;
  }
  try {
    return JSON.parse(text) as AlarmEvidence;
  } catch (err) {
    return null;
  }
}

/** 把读数拼成 chip 文案；null / 缺字段一律不显示（不能用 0 冒充读数） */
function evidenceTexts(ev: AlarmEvidence | null): string[] {
  const out: string[] = [];
  if (ev === null) {
    return out;
  }
  if (ev.temperature !== null && ev.temperature !== undefined) {
    out.push('温度 ' + ev.temperature.toFixed(1) + '℃');
  }
  if (ev.humidity !== null && ev.humidity !== undefined) {
    out.push('湿度 ' + ev.humidity.toFixed(1) + '%');
  }
  if (ev.lux !== null && ev.lux !== undefined) {
    out.push('光照 ' + ev.lux.toFixed(0) + 'lx');
  }
  if (ev.pirDetected !== null && ev.pirDetected !== undefined) {
    out.push(ev.pirDetected ? '人体 有活动' : '人体 无活动');
  }
  if (ev.doorOpen !== null && ev.doorOpen !== undefined) {
    out.push(ev.doorOpen ? '门磁 已开' : '门磁 已关');
  }
  if (ev.online !== null && ev.online !== undefined) {
    out.push(ev.online ? '设备 在线' : '设备 离线');
  }
  return out;
}

/** 一条告警走过的四步：触发 →（超时激化）→ 确认 → 关闭 */
function timelineNodes(item: AlarmEvent): TimelineNode[] {
  const nodes: TimelineNode[] = [];
  nodes.push({ title: '触发告警', time: shortTime(item.ts), done: true });
  if (item.escalated) {
    nodes.push({ title: '超时未确认，自动升级', time: '已升级', done: true });
  }
  nodes.push({
    title: item.ackTs === null ? '等待家属确认' : '家属已确认',
    time: item.ackTs === null ? '—' : shortTime(item.ackTs),
    done: item.ackTs !== null
  });
  nodes.push({
    title: item.closedTs === null ? '尚未关闭' : '处置完成并关闭',
    time: item.closedTs === null ? '—' : shortTime(item.closedTs),
    done: item.closedTs !== null
  });
  return nodes;
}

@Component
export struct AlarmTab {
  /** 关怀模式（大字）开关，由「我的」页写入 AppStorage */
  @StorageLink('largeFont') largeFont: boolean = false;
  /** 当前选中的老人/设备；"全部设备"模式下用它做“只看这台”的快捷筛选 */
  /** 当前设备：切换后要立刻重取，否则列表还停在上一位老人身上 */
  @StorageLink('deviceId') @Watch('onDeviceChange') curDevice: string = '';
  /** 全局刷新信号（切换设备 / 登记新设备后 +1） */
  @StorageLink('dataVersion') @Watch('onDeviceChange') dataVersion: number = 0;

  @Prop @Watch('onTick') tick: number = 0;
  @State filter: string = 'pending';
  /** true=看所有设备（多老人/多设备场景默认），false=只看当前设备 */
  @State scopeAll: boolean = true;
  @State items: AlarmEvent[] = [];
  @State detail: AlarmEvent | null = null;
  @State offline: boolean = false;
  @State handleType: string = 'visited';
  @State resultText: string = '已上门查看，老人安好';
  @State busy: boolean = false;
  @State toast: string = '';

  aboutToAppear(): void {
    this.load();
  }

  /** 设备切换或全局刷新信号变化：重新取一次列表（详情页打开时不动，避免打断处理） */
  onDeviceChange(): void {
    if (this.detail === null) {
      this.load();
    }
  }

  onTick(): void {
    if (this.detail === null) {
      this.load();
    }
  }

  async load(): Promise<void> {
    const wanted: string = this.scopeAll ? '' : this.curDevice;
    const remote: AlarmEvent[] | null = await ApiClient.alarms(this.filter, wanted);
    if (remote === null) {
      this.offline = true;
      this.items = await AlarmStore.query(this.filter);
      return;
    }
    this.offline = false;
    // 按等级从高到低排序，L3 排最前；同等级按时间倒序
    const order: Record<string, number> = { 'L3': 3, 'L2': 2, 'L1': 1 };
    remote.sort((a: AlarmEvent, b: AlarmEvent) => {
      const la: number = order[a.level] === undefined ? 0 : order[a.level];
      const lb: number = order[b.level] === undefined ? 0 : order[b.level];
      if (la !== lb) {
        return lb - la;
      }
      return a.ts < b.ts ? 1 : -1;
    });
    this.items = remote;
    await AlarmStore.saveMany(remote);
  }

  async confirmAlarm(): Promise<void> {
    if (this.detail === null) {
      return;
    }
    const id: string = this.detail.eventId;

    // 离线：不硬拦，写进待补发队列，本地先乐观标记已确认。
    // 这里不改网关台账（也改不了），恢复后补发，网关以自己库为准。
    if (this.offline) {
      const queued: boolean = await Outbox.enqueue(id, 'ack', '', '');
      if (queued) {
        await AlarmStore.patchStatus(id, 'acknowledged',
          this.detail.handleType === null ? '' : this.detail.handleType,
          this.detail.handleResult === null ? '' : this.detail.handleResult,
          new Date().toISOString());
        this.toast = '网关不可达：已加入待补发队列，联网后自动提交';
      } else {
        this.toast = '离线且本地队列不可用，请等网络恢复后再确认';
      }
      this.detail = null;
      await this.load();
      return;
    }

    this.busy = true;
    const ok: boolean = await ApiClient.ack(id);
    this.busy = false;

    // 不论成功与否都退回列表并重新拉取：
    // 成功 -> 该条移到“处理中”；409（已被确认过）-> 列表也能纠正过来。
    this.detail = null;
    await this.load();

    if (!ok) {
      // 请求超时也会落到这里：同样入队，避免“点了没反应就得再点一次”
      const queued: boolean = await Outbox.enqueue(id, 'ack', '', '');
      this.toast = queued
        ? '提交超时：已加入待补发队列，网络恢复后自动提交'
        : '该告警状态已变化，列表已刷新';
    }
  }

  async submitClose(): Promise<void> {
    if (this.detail === null) {
      return;
    }
    if (this.resultText.length === 0) {
      this.toast = '请填写处置结果';
      return;
    }
    const id: string = this.detail.eventId;
    const handleType: string = this.handleType;
    const result: string = this.resultText;

    if (this.offline) {
      const queued: boolean = await Outbox.enqueue(id, 'close', handleType, result);
      if (queued) {
        await AlarmStore.patchStatus(id, 'closed', handleType, result, new Date().toISOString());
        this.toast = '网关不可达：处置已暂存，联网后自动提交';
      } else {
        this.toast = '离线且本地队列不可用，请等网络恢复后再提交';
      }
      this.detail = null;
      await this.load();
      return;
    }

    this.busy = true;
    const ok: boolean = await ApiClient.close(id, handleType, result);
    this.busy = false;

    this.detail = null;
    await this.load();

    if (!ok) {
      const queued: boolean = await Outbox.enqueue(id, 'close', handleType, result);
      this.toast = queued
        ? '提交超时：处置已暂存，网络恢复后自动提交'
        : '该告警状态已变化，列表已刷新';
    }
  }

  async refreshDetail(): Promise<void> {
    if (this.detail === null) {
      return;
    }
    const id: string = this.detail.eventId;
    const all: AlarmEvent[] | null = await ApiClient.alarms('');
    if (all === null) {
      return;
    }
    for (let i = 0; i < all.length; i++) {
      if (all[i].eventId === id) {
        this.detail = all[i];
        return;
      }
    }
  }

  openDetail(item: AlarmEvent): void {
    this.detail = item;
    this.toast = '';
    // 播报：只在"未处理完"的告警上念，已关闭的不再打扰
    if (item.status !== 'closed') {
      const urgent: boolean = item.level === 'L3';
      const text: string = urgent
        ? '警告，监测到' + typeLabel(item.type) + '，请立即处理'
        : '提醒，监测到' + typeLabel(item.type) + '，请及时确认';
      // 失败已由 TtsManager 内部兜底（模拟器无引擎时静默），这里不会抛异常
      TtsManager.getInstance().speak(text, urgent);
    }
    if (item.handleType !== null) {
      this.handleType = item.handleType;
    }
    if (item.handleResult !== null) {
      this.resultText = item.handleResult;
    }
  }

  /**
   * 一键呼叫家属：优先拉起系统拨号盘并预填号码（不需要权限）。
   * 设备上没有拨号应用（模拟器/平板）时，弹窗把号码显示出来并支持复制，
   * 而不是只弹一个 2 秒就消失的 toast —— 家属要能真看见号码。
   */
  callFamily(): void {
    try {
      const ctx = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const number: string = Settings.familyPhone;
      CallUtils.dial(ctx, number).then((ok: boolean) => {
        if (ok) {
          return;
        }
        this.getUIContext().showAlertDialog({
          title: '这台设备没有拨号应用',
          message: '家属号码：' + number + '（已复制到剪贴板）\n\n模拟器/平板没有电话应用，请用手机拨打；真机点这个按钮会直接拉起拨号盘。',
          primaryButton: {
            value: '再复制一次',
            action: () => {
              CallUtils.copyOnly(number);
            }
          },
          secondaryButton: {
            value: '知道了',
            action: () => {
            }
          }
        });
      });
    } catch (err) {
      // 理论上不会到这里；保留兜底，避免页面崩溃
      console.error('[AlarmTab] callFamily 异常：' + JSON.stringify(err));
    }
  }

  /* ---------------------------------------------------------------- */
  @Builder
  filterButton(label: string, value: string) {
    Text(label)
      .fontSize(Theme.fs(14, this.largeFont))
      .fontWeight(this.filter === value ? FontWeight.Bold : FontWeight.Normal)
      .fontColor(this.filter === value ? '#FFFFFF' : Theme.muted)
      .backgroundColor(this.filter === value ? Theme.navy : Theme.card)
      .borderRadius(20)
      .padding({ left: 16, right: 16, top: 8, bottom: 8 })
      .border({ width: 1, color: this.filter === value ? Theme.navy : Theme.line })
      .onClick(() => {
        this.filter = value;
        this.load();
      })
  }

  @Builder
  infoRow(label: string, value: string) {
    Row() {
      Text(label).fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted).width(72)
      Text(value).fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.slate).layoutWeight(1)
    }
    .width('100%')
    .padding({ top: 7, bottom: 7 })
  }

  /** 证据链：触发那一刻的读数，答辩时用来对账“凭什么判它” */
  @Builder
  evidenceBlock(item: AlarmEvent) {
    Column() {
      Text('触发时数据')
        .fontSize(Theme.fs(13, this.largeFont))
        .fontWeight(FontWeight.Medium)
        .fontColor(Theme.navy)
        .width('100%')
      Flex({ wrap: FlexWrap.Wrap }) {
        ForEach(evidenceTexts(parseEvidence(item.evidence)), (text: string) => {
          Text(text)
            .fontSize(Theme.fs(12, this.largeFont))
            .fontColor(Theme.slate)
            .backgroundColor(Theme.bg)
            .borderRadius(10)
            .padding({ left: 10, right: 10, top: 5, bottom: 5 })
            .margin({ right: 8, top: 8 })
        }, (text: string) => text)
      }
      .width('100%')
    }
    .width('100%')
    .padding(16)
    .backgroundColor(Theme.card)
    .borderRadius(Theme.radius)
    .border({ width: 1, color: Theme.line })
  }

  /** 处置时间线：谁在什么时候做了什么，一条线看完 */
  @Builder
  timelineBlock(item: AlarmEvent) {
    Column() {
      Text('处置时间线')
        .fontSize(Theme.fs(13, this.largeFont))
        .fontWeight(FontWeight.Medium)
        .fontColor(Theme.navy)
        .width('100%')
        .margin({ bottom: 10 })
      ForEach(timelineNodes(item), (node: TimelineNode, index: number) => {
        Row() {
          Column() {
            Column()
              .width(10)
              .height(10)
              .borderRadius(5)
              .backgroundColor(node.done ? Theme.blue : Theme.line)
              .border({ width: 2, color: node.done ? Theme.blue : '#B6C2D2' })
            if (index < timelineNodes(item).length - 1) {
              Column()
                .width(2)
                .height(26)
                .backgroundColor(node.done ? Theme.blue : Theme.line)
            }
          }
          .width(20)
          .alignItems(HorizontalAlign.Center)

          Column() {
            Text(node.title)
              .fontSize(Theme.fs(13, this.largeFont))
              .fontColor(node.done ? Theme.navy : Theme.muted)
              .width('100%')
            Text(node.time)
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 2 })
          }
          .layoutWeight(1)
          .margin({ left: 10 })
        }
        .width('100%')
        .alignItems(VerticalAlign.Top)
      }, (node: TimelineNode, index: number) => node.title + index)
    }
    .width('100%')
    .padding(16)
    .backgroundColor(Theme.card)
    .borderRadius(Theme.radius)
    .border({ width: 1, color: Theme.line })
  }

  @Builder
  handleChip(label: string, value: string) {
    Text(label)
      .fontSize(Theme.fs(12, this.largeFont))
      .fontColor(this.handleType === value ? '#FFFFFF' : Theme.slate)
      .backgroundColor(this.handleType === value ? Theme.blue : Theme.bg)
      .borderRadius(14)
      .padding({ left: 12, right: 12, top: 6, bottom: 6 })
      .margin({ right: 8, bottom: 8 })
      .enabled(!this.offline)
      .onClick(() => {
        this.handleType = value;
      })
  }

  @Builder
  alarmCard(item: AlarmEvent) {
    Row() {
      // 等级色条改为 Row 的左边框（见下方 .border），不再用 height('100%') 的子元素：
      // 列表只有一条时，百分比高度会按 List 给的"撑满"约束解析，把卡片拉成半屏高。
      Column() {
        Text(`${levelLabel(item.level)} · ${item.message}`)
          .fontSize(Theme.fs(15, this.largeFont))
          .fontWeight(FontWeight.Bold)
          .fontColor(Theme.navy)
          .maxLines(2)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .width('100%')

        Row() {
          Text(shortTime(item.ts)).fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted)
          if (item.deviceId.length > 0) {
            Text(item.deviceId)
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(Theme.slate)
              .backgroundColor('#F1F5F9')
              .borderRadius(6)
              .padding({ left: 6, right: 6, top: 1, bottom: 1 })
              .margin({ left: 8 })
          }
          if (item.status === 'pending') {
            Text('新')
              .fontSize(Theme.fs(10, this.largeFont))
              .fontColor('#FFFFFF')
              .backgroundColor(Theme.orange)
              .borderRadius(8)
              .padding({ left: 6, right: 6, top: 1, bottom: 1 })
              .margin({ left: 8 })
          }
          Text(statusLabel(item.status)).fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.blue).margin({ left: 10 })
          Blank()
          Text(sourceLabel(item.source))
            .fontSize(Theme.fs(11, this.largeFont))
            .fontColor(isHardware(item.source) ? Theme.green : Theme.orange)
        }
        .width('100%')
        .margin({ top: 8 })
      }
      .layoutWeight(1)
    }
    .width('100%')
    .padding(14)
    .backgroundColor(item.status === 'pending' ? '#FFF7F5' : Theme.card)
    .borderRadius(Theme.radius)
    .border({
      width: { left: 4, top: 1, right: 1, bottom: 1 },
      color: {
        left: Theme.levelColor(item.level),
        top: item.status === 'pending' ? Theme.orange : Theme.line,
        right: item.status === 'pending' ? Theme.orange : Theme.line,
        bottom: item.status === 'pending' ? Theme.orange : Theme.line
      }
    })
    .onClick(() => {
      this.openDetail(item);
    })
  }

  /* ---------------------------------------------------------------- */
  @Builder
  detailPage(item: AlarmEvent) {
    Column() {
      Row() {
        Text('‹ 返回').fontSize(Theme.fs(16, this.largeFont)).fontColor('#FFFFFF')
          .onClick(() => {
            this.detail = null;
          })
        Blank()
        Text(statusLabel(item.status)).fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.blueLight)
      }
      .width('100%')
      .padding(Theme.page)
      .backgroundColor(Theme.navy)

      Scroll() {
        Column({ space: Theme.gap }) {
          Column() {
            Row() {
              Text(levelLabel(item.level))
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor('#FFFFFF')
                .backgroundColor(Theme.levelColor(item.level))
                .borderRadius(10)
                .padding({ left: 8, right: 8, top: 3, bottom: 3 })
              Text(typeLabel(item.type)).fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ left: 8 })
            }
            .width('100%')

            Text(item.message)
              .fontSize(Theme.fs(20, this.largeFont))
              .fontWeight(FontWeight.Bold)
              .fontColor(Theme.navy)
              .width('100%')
              .margin({ top: 10 })
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          Column() {
            this.infoRow('触发时间', shortTime(item.ts))
            this.infoRow('区域', item.zone === null ? '未知区域' : item.zone)
            this.infoRow('数据来源', sourceLabel(item.source))
            this.infoRow('告警编号', item.eventId)
            if (item.ackTs !== null) {
              this.infoRow('确认时间', shortTime(item.ackTs))
            }
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          this.evidenceBlock(item)
          this.timelineBlock(item)

          if (!isHardware(item.source)) {
            Text('⚠ 本条为模拟/测试数据，不是真实传感器采集')
              .fontSize(Theme.fs(12, this.largeFont))
              .fontColor(Theme.orange)
              .width('100%')
              .textAlign(TextAlign.Center)
          }

          if (item.status === 'closed') {
            Column() {
              Text('处置结果').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted).width('100%')
              Text(handleTypeLabel(item.handleType))
                .fontSize(Theme.fs(16, this.largeFont))
                .fontWeight(FontWeight.Bold)
                .fontColor(Theme.navy)
                .width('100%')
                .margin({ top: 8 })
              Text(item.handleResult === null ? '—' : item.handleResult)
                .fontSize(Theme.fs(13, this.largeFont))
                .fontColor(Theme.slate)
                .width('100%')
                .margin({ top: 6 })
              Text(`关闭时间 ${shortTime(item.closedTs)}`)
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
          } else {
            Column() {
              // 建议措施：告诉用户此刻最该做什么
              Column() {
                Text('建议措施').fontSize(Theme.fs(13, this.largeFont)).fontWeight(FontWeight.Medium)
                  .fontColor(Theme.navy).width('100%')
                Text(adviceFor(this.detail === null ? '' : this.detail.type))
                  .fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.slate).lineHeight(20)
                  .width('100%').margin({ top: 6 })
              }
              .width('100%').padding(14).backgroundColor('#FFFBEB')
              .borderRadius(Theme.radius).border({ width: 1, color: Theme.orange })
              .margin({ bottom: 12 })

              // 一键呼叫家人：真出事了最快的是打电话，而不是继续在 APP 里点
              Button('一键呼叫家属')
                .width('100%')
                .height(52)
                .fontSize(Theme.fs(17, this.largeFont))
                .fontWeight(FontWeight.Bold)
                .fontColor('#FFFFFF')
                .backgroundColor(Theme.red)
                .borderRadius(12)
                .margin({ bottom: 12 })
                .onClick(() => {
                  this.callFamily();
                })

              Text('处置结果（必填，≤200 字）').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted).width('100%')
              TextInput({ text: this.resultText, placeholder: '例如：已联系社区网格员上门查看' })
                .fontSize(Theme.fs(13, this.largeFont))
                .height(42)
                .margin({ top: 8 })
                .enabled(!this.offline)
                .onChange((value: string) => {
                  this.resultText = value;
                })

              Flex({ wrap: FlexWrap.Wrap }) {
                this.handleChip('已上门查看', 'visited')
                this.handleChip('电话确认', 'phone')
                this.handleChip('误报', 'false_alarm')
                this.handleChip('转社区', 'community')
                this.handleChip('其他', 'other')
              }
              .margin({ top: 10 })

              if (item.status === 'pending') {
                Button(this.busy ? '处理中…' : '确认告警')
                  .width('100%')
                  .height(42)
                  .fontSize(Theme.fs(15, this.largeFont))
                  .backgroundColor(Theme.blue)
                  .fontColor('#FFFFFF')
                  .borderRadius(12)
                  .enabled(!this.offline && !this.busy)
                  .onClick(() => {
                    this.confirmAlarm();
                  })
              }

              Button(this.busy ? '提交中…' : '提交处置并关闭')
                .width('100%')
                .height(46)
                .fontSize(Theme.fs(16, this.largeFont))
                .fontWeight(FontWeight.Bold)
                .backgroundColor(Theme.orange)
                .fontColor('#FFFFFF')
                .borderRadius(12)
                .margin({ top: 10 })
                .enabled(!this.offline && !this.busy)
                .onClick(() => {
                  this.submitClose();
                })

              if (this.offline) {
                Text('离线状态只能查看，处置需要连接网关')
                  .fontSize(Theme.fs(11, this.largeFont))
                  .fontColor(Theme.orange)
                  .width('100%')
                  .textAlign(TextAlign.Center)
                  .margin({ top: 8 })
              }
              if (this.toast.length > 0) {
                Text(this.toast)
                  .fontSize(Theme.fs(12, this.largeFont))
                  .fontColor(Theme.orange)
                  .width('100%')
                  .textAlign(TextAlign.Center)
                  .margin({ top: 8 })
              }
            }
            .width('100%')
            .padding(16)
            .backgroundColor(Theme.card)
            .borderRadius(Theme.radius)
            .border({ width: 1, color: Theme.line })
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

  build() {
    if (this.detail !== null) {
      this.detailPage(this.detail)
    } else {
      Column() {
        Row() {
          Text('告警').fontSize(Theme.fs(20, this.largeFont)).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
          Blank()
          Text(`${this.items.length} 条`).fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.blueLight)
        }
        .width('100%')
        .padding(Theme.page)
        .backgroundColor(Theme.navy)

        Row({ space: 10 }) {
          ForEach(FILTER_VALUES, (value: string, index: number) => {
            this.filterButton(FILTER_LABELS[index], value)
          }, (value: string) => value)
        }
        .width('100%')
        .padding({ left: Theme.page, right: Theme.page, top: 12, bottom: 4 })

        Row() {
          Text(this.scopeAll ? '全部设备' : ('仅 ' + (this.curDevice.length > 0 ? this.curDevice : '当前设备')))
            .fontSize(Theme.fs(12, this.largeFont))
            .fontColor(Theme.blue)
            .padding({ left: 12, right: 12, top: 5, bottom: 5 })
            .backgroundColor('#EFF6FF')
            .borderRadius(12)
            .onClick(() => {
              this.scopeAll = !this.scopeAll;
              this.load();
            })
          Text(this.scopeAll ? '点一下只看当前设备' : '点一下看全部设备')
            .fontSize(Theme.fs(11, this.largeFont))
            .fontColor(Theme.muted)
            .margin({ left: 10 })
        }
        .width('100%')
        .padding({ left: Theme.page, right: Theme.page, top: 4, bottom: 4 })

        if (this.offline) {
          Text('离线：显示本地缓存的告警，数据可能不是最新')
            .fontSize(Theme.fs(11, this.largeFont))
            .fontColor(Theme.amber)
            .width('100%')
            .textAlign(TextAlign.Center)
            .padding({ top: 6 })
        }

        if (this.items.length === 0) {
          Column() {
            Text(this.offline ? '离线且本地无缓存' : '暂无告警').fontSize(Theme.fs(15, this.largeFont)).fontColor(Theme.muted)
            Text('触发“长时间无活动”或按求助键后，这里会出现记录')
              .fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.muted).margin({ top: 8 })
          }
          .width('100%')
          .padding({ top: 80 })
        } else {
          List({ space: 10 }) {
            ForEach(this.items, (item: AlarmEvent) => {
              ListItem() {
                this.alarmCard(item)
              }
            }, (item: AlarmEvent) => item.eventId + item.status)
          }
          .layoutWeight(1)
          .padding({ left: Theme.page, right: Theme.page, top: 8, bottom: 8 })
          .scrollBar(BarState.Off)
        }
      }
      .width('100%')
      .height('100%')
      .backgroundColor(Theme.bg)
    }
  }
}
