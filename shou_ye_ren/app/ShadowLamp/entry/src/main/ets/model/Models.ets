/**
 * 数据模型定义
 */

export interface ActiveAlarm {
  eventId: string;
  level: string;
  type: string;
  message: string;
  ts: string;
}

export interface SensorSnapshot {
  deviceId: string;
  ts: string;
  seq: number;
  pirDetected: boolean;
  lastMotionTs: string | null;
  lastMotionZone: string | null;
  temperature: number | null;
  humidity: number | null;
  lux: number | null;
  doorOpen: boolean | null;
  battery: number | null;
  powerState: string | null;
  rssi: number | null;
  online: boolean;
  source: string;
  fwVersion: string;
  staleSeconds?: number;
  /** 最高等级的待确认告警；null 表示没有待处理的事 */
  activeAlarm?: ActiveAlarm | null;
}

export interface AlarmEvent {
  eventId: string;
  deviceId: string;
  ts: string;
  type: string;
  level: string;
  message: string;
  status: string;
  ackTs: string | null;
  ackOperator: string | null;
  handleType: string | null;
  handleResult: string | null;
  closedTs: string | null;
  zone: string | null;
  source: string;
  escalated: boolean;
  /** 触发告警时的传感器数值快照 */
  evidence?: string | null;
  /** 告警入网时间（网关落库时刻），时间线里区分“触发”与“升级”用 */
  createdTs?: string | null;
}

/** evidence 解出来的结构：字段可能缺，缺就是不显示，不拿 0 冒充 */
export interface AlarmEvidence {
  temperature?: number | null;
  humidity?: number | null;
  lux?: number | null;
  pirDetected?: boolean | null;
  doorOpen?: boolean | null;
  online?: boolean | null;
}

export interface DailyReport {
  date: string;
  totalCount: number;
  hourly: number[];
  deltaVsYesterday: number | null;
  peakHour: number | null;
  level: string;
  note: string;
  baseline: number | null;
  alarmCount: number;
}

export interface WeeklyReport {
  week: string;
  startDate: string;
  endDate: string;
  totalCount: number;
  dailyAvg: number;
  dailyCounts: number[];
  deltaVsLastWeek: number | null;
  alarmCount: number;
  tempAvg: number | null;
  humAvg: number | null;
}

/**
 * 月报：与周报同一套口径（总活动 / 日均 / 告警），
 * 另外给出当月平均温湿度；当月无采集时后端返回 null，界面显示“—”。
 */
export interface MonthlyReport {
  month: string;            /* 形如 2026-10 */
  days: number;             /* 统计天数 */
  totalCount: number;       /* 当月活动总数 */
  dailyAvg: number;         /* 日均活动 */
  alarmCount: number;       /* 当月告警数 */
  tempAvg: number | null;   /* 平均温度，可能为 null */
  humAvg: number | null;    /* 平均湿度，可能为 null */
}

export interface DeviceStatus {
  deviceId: string;
  online: boolean;
  battery: number | null;
  powerState: string | null;
  rssi: number | null;
  fwVersion: string;
  lastSeen: string;
  uptime: number | null;
  staleSeconds: number | null;
  /** 这台设备当前有几条待确认告警（网关 /api/v1/devices 下发，多设备时用来判断"谁出事了"） */
  pendingAlarms?: number;
  /**
   * 这台设备最近一帧数据的来源：hardware=真实设备，test=模拟演示，null=只登记还没上报。
   * 由网关 /api/v1/devices 下发（store._latest_source），APP 只做展示，不自己猜。
   */
  source?: string | null;
}

/** GET /api/v1/devices 的返回：{"items":[...],"count":n} */
export interface DeviceListResult {
  items: DeviceStatus[];
  count: number;
}

export interface AlarmListResult {
  items: AlarmEvent[];
  total: number;
}

const ZONE_LABEL: Record<string, string> = {
  'living': '客厅',
  'bedroom': '卧室',
  'bathroom': '卫生间',
  'kitchen': '厨房',
  'entry': '入户'
};

/** 区域名；无法定位时返回空串，由界面决定“不显示”，而不是硬写“未知区域” */
export function zoneLabel(zone: string | null): string {
  if (zone === null || zone === '' || zone === 'unknown') {
    return '';
  }
  const label: string = ZONE_LABEL[zone];
  return label === undefined ? zone : label;
}

export function levelLabel(level: string): string {
  if (level === 'L3') {
    return '紧急';
  }
  if (level === 'L2') {
    return '关注';
  }
  return '提醒';
}

export function statusLabel(status: string): string {
  if (status === 'pending') {
    return '待确认';
  }
  if (status === 'acknowledged') {
    return '处理中';
  }
  return '已关闭';
}

/** 来源标签转换 */
export function sourceLabel(source: string): string {
  if (source === 'hardware' || source === 'real') {
    return '真实硬件';
  }
  if (source === 'key_sim') {
    return '按键模拟';
  }
  if (source === 'test') {
    return '测试/模拟';
  }
  return '未知来源';
}

/** 是否真实硬件来源（决定是否显示“模拟”警示） */
export function isHardware(source: string): boolean {
  return source === 'hardware' || source === 'real';
}

export function handleTypeLabel(type: string | null): string {
  if (type === 'visited') {
    return '已上门查看';
  }
  if (type === 'phone') {
    return '电话确认';
  }
  if (type === 'false_alarm') {
    return '误报';
  }
  if (type === 'community') {
    return '转社区网格员';
  }
  if (type === 'other') {
    return '其他';
  }
  return '—';
}

/** "2026-11-02T14:03:00+08:00" -> "11-02 14:03" */
export function shortTime(iso: string | null): string {
  if (iso === null || iso.length < 16) {
    return '—';
  }
  return iso.substring(5, 16).replace('T', ' ');
}

/**
 * 距某个 ISO 时间点过了多少秒。
 * 用途：首页“最后活动”要基于 lastMotionTs，而不是数据新鲜度（staleSeconds），
 * 这两者混用会把“设备很久没上报”误显示成“老人很久没活动”。
 */
export function secondsSince(iso: string | null): number | null {
  if (iso === null || iso.length === 0) {
    return null;
  }
  const ts: number = new Date(iso).getTime();
  if (isNaN(ts)) {
    return null;
  }
  return Math.max(0, Math.floor((Date.now() - ts) / 1000));
}

/** 把秒数说成人话：3 分钟前 / 2 小时前 */
export function humanAgo(seconds: number | null | undefined): string {
  if (seconds === null || seconds === undefined) {
    return '—';
  }
  if (seconds < 5) {
    return '刚刚';
  }
  if (seconds < 60) {
    return `${Math.floor(seconds)} 秒前`;
  }
  if (seconds < 3600) {
    return `${Math.floor(seconds / 60)} 分钟前`;
  }
  return `${Math.floor(seconds / 3600)} 小时前`;
}


/** 告警类型中文名：列表和详情里都用它，避免出现 TEMP_FIRE 这类英文 */
const TYPE_LABEL: Record<string, string> = {
  'SOS': '紧急求助',
  'GAS': '燃气泄漏',
  'SMOKE': '烟雾报警',
  'TEMP_FIRE': '疑似火情',
  'TEMP_HIGH': '温度过高',
  'TEMP_WARM': '温度偏高',
  'TEMP_LOW': '温度过低',
  'HUM_HIGH': '湿度过高',
  'HUM_WARN_HIGH': '湿度偏潮',
  'HUM_LOW': '湿度过低',
  'HUM_WARN_LOW': '湿度偏干',
  'LUX_NIGHT_HIGH': '夜间强光',
  'LUX_DAY_LOW': '采光不足',
  'NO_MOTION': '长时间无活动',
  'SLEEP_GAP': '睡眠期静默',
  'DOOR_OPEN': '门窗未关',
  'AWAY_LONG': '外出未归'
};

export function typeLabel(type: string): string {
  const label: string = TYPE_LABEL[type];
  return label === undefined ? type : label;
}


/** 遇到该类告警时，最有用的一两步先做什么 */
export function adviceFor(type: string): string {
  if (type === 'SOS') {
    return '先打电话确认老人状态；无人接听立即联系邻居或上门，必要时拨打 120。';
  }
  if (type === 'GAS') {
    return '先开窗、不要开关电器和打电话，关闭燃气总阀后撤离到室外再报警。';
  }
  if (type === 'SMOKE') {
    return '先确认是否有明火和烟雾来源，能安全处置就断电断气，不能就撤离并拨打 119。';
  }
  if (type === 'TEMP_FIRE') {
    return '先确认是否真的起火（看烟雾、闻气味）；有火情立即撤离并拨打 119，不要先收拾物品。';
  }
  if (type === 'TEMP_HIGH' || type === 'TEMP_WARM') {
    return '检查取暖设备是否长时间开启，开窗通风；老人出现头晕、乏力要立即降温补水并联系医生。';
  }
  if (type === 'TEMP_LOW') {
    return '提醒添衣、检查取暖设备；低温对老人心脑血管风险高，持续偏低要上门查看。';
  }
  if (type === 'HUM_HIGH' || type === 'HUM_WARN_HIGH') {
    return '开窗或开除湿机，检查是否有渗水；地面潮湿容易滑倒，提醒穿防滑鞋。';
  }
  if (type === 'HUM_LOW' || type === 'HUM_WARN_LOW') {
    return '提醒多喝水、开加湿器；干燥易引起呼吸道不适和静电。';
  }
  if (type === 'NO_MOTION') {
    return '先打电话；无人接听立即联系邻居上门查看，重点确认卫生间和卧室。';
  }
  if (type === 'SLEEP_GAP') {
    return '先轻敲门或打电话叫醒确认；长时间无回应要上门，避免低血糖或突发疾病被忽略。';
  }
  if (type === 'DOOR_OPEN') {
    return '确认是忘关门还是人已外出；若老人独居且长时间未关，建议上门确认。';
  }
  if (type === 'AWAY_LONG') {
    return '确认是否为正常外出；超时未归可联系家属或社区网格员协助寻找。';
  }
  if (type === 'LUX_NIGHT_HIGH') {
    return '夜间强光可能是起夜未关灯，或有异常情况；可远程提醒，必要时上门看看。';
  }
  if (type === 'LUX_DAY_LOW') {
    return '白天采光不足会影响情绪和视力，建议拉开窗帘或补一盏灯。';
  }
  return '先与老人取得联系确认情况；联系不上再联系家属或上门。';
}

/** 家庭组成员：role 是网关给的英文枚举，roleText 是给人看的中文。 */
export interface FamilyMember {
  memberId: string;
  name: string;
  role: string;
  roleText: string;
  joinedAt: string;
  isOwner: boolean;
}

/** 一位老人 = 一个家庭组；组内多人共享同一份监测数据。 */
export interface FamilyInfo {
  familyId: string;
  familyName: string;
  inviteCode: string;
  createdAt: string;
  members: FamilyMember[];
  memberCount: number;
  note: string;
}

/**
 * 设备登记结果：把三种失败分开，才能给出对得上的提示。
 * （原来只返回 boolean，界面上只能笼统一句“登记失败”，老人和家属都看不懂。）
 */
export interface DeviceAddResult {
  /** ok=已登记；offline=连不上网关；bad=编号不合法/网关拒绝 */
  kind: string;
  /** 给用户看的中文原因（ok 时为空串） */
  message: string;
}
