/**
 * 待提交队列（离线操作补发）。
 *
 * 为什么需要：独居老人家里 WiFi 时好时坏，家属在现场按"确认"时如果正好断网，
 * 操作直接丢掉会让台账和现场对不上（老人已经被看过了，系统还挂"待确认"，
 * 板子也会一直响）。所以断网时先把操作写进本地队列，网关一恢复就按顺序补发。
 *
 * 实现取舍：
 *  - 队列只存"幂等"的两种动作：确认、提交处置；两者重复提交网关都是幂等的
 *    （重复确认返回 409 + 当前状态，重复关闭同样返回当前状态），补发不会写乱；
 *  - 不存整条告警内容，只存 eventId + 动作 + 参数，网关以自己库里的为准；
 *  - 补发按入队顺序 @1 条一条来，第一条失败就停下（说明还没通），不空转。
 */
import { relationalStore } from '@kit.ArkData';
import { common } from '@kit.AbilityKit';
import { ApiClient } from './ApiClient';

const CREATE_SQL: string = 'CREATE TABLE IF NOT EXISTS pending_op (' +
  'id INTEGER PRIMARY KEY AUTOINCREMENT, eventId TEXT, action TEXT, ' +
  'handleType TEXT, handleResult TEXT, ts TEXT)';

/** 队列里的一行 */
export interface PendingOp {
  id: number;
  eventId: string;
  action: string;
  handleType: string;
  handleResult: string;
  ts: string;
}

export class Outbox {
  private static store: relationalStore.RdbStore | null = null;
  private static ready: boolean = false;
  /** 上次补发失败的次数，用于退避：连续失败就不每 9 秒硬试 */
  private static fails: number = 0;

  static async init(context: common.UIAbilityContext): Promise<void> {
    if (Outbox.ready) {
      return;
    }
    try {
      const config: relationalStore.StoreConfig = {
        name: 'shadowlamp.db',
        securityLevel: relationalStore.SecurityLevel.S1
      };
      const store: relationalStore.RdbStore = await relationalStore.getRdbStore(context, config);
      await store.executeSql(CREATE_SQL);
      Outbox.store = store;
      Outbox.ready = true;
    } catch (err) {
      Outbox.ready = false;
    }
    AppStorage.setOrCreate<number>('pendingOps', await Outbox.count());
  }

  static isReady(): boolean {
    return Outbox.ready && Outbox.store !== null;
  }

  /** 入队：action 只允许 'ack' / 'close' */
  static async enqueue(eventId: string, action: string, handleType: string,
                       handleResult: string): Promise<boolean> {
    if (!Outbox.isReady()) {
      return false;
    }
    const store: relationalStore.RdbStore = Outbox.store as relationalStore.RdbStore;
    const ts: string = new Date().toISOString();
    try {
      await store.executeSql(
        'INSERT INTO pending_op (eventId, action, handleType, handleResult, ts) VALUES (?,?,?,?,?)',
        [eventId, action, handleType, handleResult, ts]);
    } catch (err) {
      return false;
    }
    AppStorage.setOrCreate<number>('pendingOps', await Outbox.count());
    return true;
  }

  static async count(): Promise<number> {
    if (!Outbox.isReady()) {
      return 0;
    }
    const store: relationalStore.RdbStore = Outbox.store as relationalStore.RdbStore;
    try {
      const rs: relationalStore.ResultSet = await store.querySql('SELECT COUNT(*) FROM pending_op');
      let n: number = 0;
      try {
        if (rs.goToNextRow()) {
          n = rs.getLong(0);
        }
      } finally {
        rs.close();
      }
      return n;
    } catch (err) {
      return 0;
    }
  }

  private static async list(): Promise<PendingOp[]> {
    const store: relationalStore.RdbStore = Outbox.store as relationalStore.RdbStore;
    const items: PendingOp[] = [];
    const rs: relationalStore.ResultSet = await store.querySql(
      'SELECT id, eventId, action, handleType, handleResult, ts FROM pending_op ORDER BY id ASC');
    try {
      while (rs.goToNextRow()) {
        const item: PendingOp = {
          id: rs.getLong(rs.getColumnIndex('id')),
          eventId: rs.getString(rs.getColumnIndex('eventId')),
          action: rs.getString(rs.getColumnIndex('action')),
          handleType: rs.getString(rs.getColumnIndex('handleType')),
          handleResult: rs.getString(rs.getColumnIndex('handleResult')),
          ts: rs.getString(rs.getColumnIndex('ts'))
        };
        items.push(item);
      }
    } finally {
      rs.close();
    }
    return items;
  }

  private static async remove(id: number): Promise<void> {
    const store: relationalStore.RdbStore = Outbox.store as relationalStore.RdbStore;
    await store.executeSql('DELETE FROM pending_op WHERE id=?', [id]);
  }

  /**
   * 补发：返回本次成功条数。
   * 连续失败 6 次（约 1 分钟）后本周期直接跳过，避免一直戳不可达的网关。
   */
  static async flush(): Promise<number> {
    if (!Outbox.isReady()) {
      return 0;
    }
    if (Outbox.fails >= 6) {
      return 0;
    }
    let success: number = 0;
    let items: PendingOp[] = [];
    try {
      items = await Outbox.list();
    } catch (err) {
      return 0;
    }
    for (let i = 0; i < items.length; i++) {
      const op: PendingOp = items[i];
      let ok: boolean = false;
      if (op.action === 'ack') {
        ok = await ApiClient.ack(op.eventId);
      } else {
        ok = await ApiClient.close(op.eventId, op.handleType, op.handleResult);
      }
      if (!ok) {
        Outbox.fails += 1;
        break;
      }
      await Outbox.remove(op.id);
      success += 1;
    }
    if (success > 0) {
      Outbox.fails = 0;
    }
    AppStorage.setOrCreate<number>('pendingOps', await Outbox.count());
    // 补发成功后把列表也刷新一遍（不在这里碰 UI，靠 tick 自然刷新）
    AppStorage.setOrCreate<number>('outboxFlushed', success);
    return success;
  }
}
