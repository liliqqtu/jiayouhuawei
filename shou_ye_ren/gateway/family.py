# -*- coding: utf-8 -*-
"""家庭组：一位老人 = 一个家庭组，组内可以有多个监护人（子女、社区网格员）。

设计取舍（写在这里，答辩时会被问）：
  1) 本期只做「局域网 + 内存态」：成员关系存在进程内存里，网关一重启就清空。
     演示场景够用（家里一台网关、一家人围着看），但**不是生产方案**——
     生产环境应替换为云端账号体系（用户实名/手机号登录、家庭组落库、邀请
     走短信或小程序），这块我们如实标注为“未实现”，不假装有。
  2) 加入密码是 6 位数字，只在同一局域网内有效；它相当于“口头邀请码”，
     不承担账号鉴权职责。真正的鉴权（谁是谁）本期也没有，安全边界是
     “只在内网、只演示用”，这一点在文档里明说。
  3) 家庭组与设备并不强绑定：组是“人”的组织，device 是“物”的台账。
     APP 里看到的还是这台网关下的设备列表，家庭组只决定“谁可以一起看”。
"""
from __future__ import annotations

import random
import threading
from datetime import datetime, timezone, timedelta

TZ = timezone(timedelta(hours=8))

ROLE_OWNER = "owner"
ROLE_GUARDIAN = "guardian"
ROLE_TEXT = {ROLE_OWNER: "户主", ROLE_GUARDIAN: "监护人"}


def _now_iso() -> str:
    return datetime.now(TZ).replace(microsecond=0).isoformat()


def _member_view(member: dict) -> dict:
    """对外成员视图：把英文 role 同时翻成中文，APP 直接显示，不用再查表。"""
    return {
        "memberId": member["memberId"],
        "name": member["name"],
        "role": member["role"],
        "roleText": ROLE_TEXT.get(member["role"], "成员"),
        "joinedAt": member["joinedAt"],
        "isOwner": member["role"] == ROLE_OWNER,
    }


def _family_view(family: dict) -> dict:
    return {
        "familyId": family["familyId"],
        "familyName": family["familyName"],
        "inviteCode": family["inviteCode"],
        "createdAt": family["createdAt"],
        "members": [_member_view(m) for m in family["members"]],
        "memberCount": len(family["members"]),
        "note": "家庭组信息保存在网关内存中，重启网关后会重置（生产环境应换云端账号体系）",
    }


class FamilyError(Exception):
    """家庭组业务错误：带上 HTTP 状态码，让 api 层原样返回。"""

    def __init__(self, status: int, code: str, message: str):
        super(FamilyError, self).__init__(message)
        self.status = status
        self.code = code
        self.message = message


class FamilyRegistry(object):
    """线程安全的家庭组注册表（内存态）。

    为什么加锁：网关是 ThreadingHTTPServer，多个手机可能同时“创建/加入”，
    不加锁会出现两个人抢到同一个 6 位码、或者成员列表丢写。
    """

    def __init__(self):
        self._lock = threading.Lock()
        self._by_id = {}      # familyId -> family dict
        self._by_code = {}    # inviteCode -> familyId
        self._seq = 0
        self._member_seq = 0

    # ---------------------------------------------------------------- 内部
    def _new_code(self) -> str:
        """生成不重复的 6 位数字邀请码（首位非 0，读起来好念）。"""
        for _ in range(200):
            code = "%06d" % random.randint(100000, 999999)
            if code not in self._by_code:
                return code
        raise FamilyError(500, "CODE_EXHAUSTED", "邀请码已用尽，请重启网关后重试")

    def _new_member(self, name: str, role: str) -> dict:
        self._member_seq += 1
        return {
            "memberId": "M-%04d" % self._member_seq,
            "name": name,
            "role": role,
            "joinedAt": _now_iso(),
        }

    # ---------------------------------------------------------------- 创建
    def create(self, family_name: str, owner_name: str) -> dict:
        family_name = (family_name or "").strip() or "我的家庭"
        owner_name = (owner_name or "").strip() or "户主本人"
        if len(family_name) > 20:
            raise FamilyError(400, "BAD_NAME", "家庭组名称最长 20 个字")
        if len(owner_name) > 12:
            raise FamilyError(400, "BAD_NAME", "成员称呼最长 12 个字")

        with self._lock:
            self._seq += 1
            family_id = "F-%03d" % self._seq
            code = self._new_code()
            family = {
                "familyId": family_id,
                "familyName": family_name,
                "inviteCode": code,
                "createdAt": _now_iso(),
                "members": [self._new_member(owner_name, ROLE_OWNER)],
            }
            self._by_id[family_id] = family
            self._by_code[code] = family_id
            return _family_view(family)

    # ---------------------------------------------------------------- 加入
    def join(self, invite_code: str, member_name: str) -> dict:
        invite_code = (invite_code or "").strip()
        member_name = (member_name or "").strip() or "家人"
        if len(invite_code) != 6 or not invite_code.isdigit():
            raise FamilyError(400, "BAD_CODE_FORMAT", "加入密码是 6 位数字，请重新输入")
        if len(member_name) > 12:
            raise FamilyError(400, "BAD_NAME", "成员称呼最长 12 个字")

        with self._lock:
            family_id = self._by_code.get(invite_code)
            if family_id is None:
                # 不区分“码不存在”和“码已过期”：都是错的，别给爆破提示
                raise FamilyError(404, "CODE_NOT_FOUND", "没有找到这个家庭组，请核对 6 位密码")
            family = self._by_id[family_id]
            for member in family["members"]:
                if member["name"] == member_name:
                    # 同一个人重复加入（换手机、重装）→ 直接返回原信息，不重复占位
                    return _family_view(family)
            family["members"].append(self._new_member(member_name, ROLE_GUARDIAN))
            return _family_view(family)

    # ---------------------------------------------------------------- 查询
    def get(self, family_id: str) -> dict:
        with self._lock:
            family = self._by_id.get((family_id or "").strip())
            if family is None:
                raise FamilyError(404, "FAMILY_NOT_FOUND", "这个家庭组不存在（网关重启后会重置）")
            return _family_view(family)

    def lookup(self, invite_code: str) -> dict:
        """加入前的预检：只看这个密码对不对、对应哪个组，不产生成员。"""
        with self._lock:
            family_id = self._by_code.get((invite_code or "").strip())
            if family_id is None:
                raise FamilyError(404, "CODE_NOT_FOUND", "没有找到这个家庭组，请核对 6 位密码")
            family = self._by_id[family_id]
            return {
                "familyId": family["familyId"],
                "familyName": family["familyName"],
                "memberCount": len(family["members"]),
            }

    def stats(self) -> dict:
        """给 /health 或看板用：当前有几个组、几个成员。"""
        with self._lock:
            return {
                "families": len(self._by_id),
                "members": sum(len(f["members"]) for f in self._by_id.values()),
            }
