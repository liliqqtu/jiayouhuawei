# -*- coding: utf-8 -*-
"""家庭组管理：多监护人加入、成员列表与管理员（户主）增删成员。"""
from __future__ import annotations

import random
import threading
from datetime import datetime, timezone, timedelta

TZ = timezone(timedelta(hours=8))

ROLE_OWNER = "owner"
ROLE_ADMIN = "admin"
ROLE_GUARDIAN = "guardian"
ROLE_TEXT = {ROLE_OWNER: "户主", ROLE_ADMIN: "管理员", ROLE_GUARDIAN: "监护人"}
# 户主天然就是管理员；另外允许户主把某个家人提升为管理员一起管成员
ADMIN_ROLES = (ROLE_OWNER, ROLE_ADMIN)
# 添加成员时允许指定的身份（户主不能被“添加”出来）
ASSIGNABLE_ROLES = (ROLE_ADMIN, ROLE_GUARDIAN)


def _now_iso() -> str:
    return datetime.now(TZ).replace(microsecond=0).isoformat()


def _member_view(member: dict) -> dict:
    """对外成员视图：把英文 role 同时翻成中文，APP 直接显示，不用再查表。"""
    role = member["role"]
    return {
        "memberId": member["memberId"],
        "name": member["name"],
        "role": role,
        "roleText": ROLE_TEXT.get(role, "成员"),
        "joinedAt": member["joinedAt"],
        "isOwner": role == ROLE_OWNER,
        # 户主默认拥有管理员权限，APP 用这个字段决定要不要显示“添加/删除成员”
        "isAdmin": role in ADMIN_ROLES,
    }


def _family_view(family: dict) -> dict:
    members = family["members"]
    owner = None
    for member in members:
        if member["role"] == ROLE_OWNER:
            owner = member
            break
    return {
        "familyId": family["familyId"],
        "familyName": family["familyName"],
        "inviteCode": family["inviteCode"],
        "createdAt": family["createdAt"],
        "ownerId": owner["memberId"] if owner is not None else "",
        "members": [_member_view(m) for m in members],
        "memberCount": len(members),
        "adminCount": len([m for m in members if m["role"] in ADMIN_ROLES]),
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

    为什么加锁：网关是 ThreadingHTTPServer，多个手机可能同时“创建/加入/删人”，
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

    def _family_locked(self, family_id: str) -> dict:
        """调用前必须已持锁：按 familyId 取组，取不到就 404。"""
        family = self._by_id.get((family_id or "").strip())
        if family is None:
            raise FamilyError(404, "FAMILY_NOT_FOUND", "这个家庭组不存在（网关重启后会重置）")
        return family

    def _find_member(self, family: dict, member_id=None, name=None) -> dict:
        for member in family["members"]:
            if member_id:
                if member["memberId"] == member_id:
                    return member
            elif name and member["name"] == name:
                return member
        return None

    def _require_admin(self, family: dict, operator_id=None, operator_name=None) -> dict:
        """校验操作者是不是管理员：户主默认是管理员，其余人要看 role。

        本期没有账号体系（局域网演示），APP 用“我的成员称呼/成员编号”自称身份，
        所以这里只做业务校验，不当作鉴权；真上线要换成登录态 + 服务端签名。
        """
        operator = self._find_member(family, member_id=operator_id, name=operator_name)
        if operator is None:
            raise FamilyError(403, "NOT_MEMBER", "操作者不在这个家庭组里，请先进入家庭组页面")
        if operator["role"] not in ADMIN_ROLES:
            raise FamilyError(403, "NOT_ADMIN", "只有户主或管理员才能添加、删除成员")
        return operator

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

    # ------------------------------------------------------- 管理员增删成员
    def add_member(self, family_id: str, operator_id=None, operator_name=None,
                   member_name: str = "", role: str = ROLE_GUARDIAN) -> dict:
        """管理员（户主）直接添加成员，不用对方扫码/输密码。

        为什么需要：老人不会操作“加入家庭组”，常见场景是户主替远在外地的亲戚
        先建好成员位，或者社工上门帮家属把账号加进来看数据。
        """
        member_name = (member_name or "").strip() or "家人"
        role = (role or ROLE_GUARDIAN).strip()
        if len(member_name) > 12:
            raise FamilyError(400, "BAD_NAME", "成员称呼最长 12 个字")
        if role not in ASSIGNABLE_ROLES:
            raise FamilyError(400, "BAD_ROLE", "成员身份只能是“监护人”或“管理员”")

        with self._lock:
            family = self._family_locked(family_id)
            self._require_admin(family, operator_id, operator_name)
            for member in family["members"]:
                if member["name"] == member_name:
                    raise FamilyError(409, "MEMBER_EXISTS",
                                      "家庭组里已经有叫“%s”的成员了，换一个称呼再添加" % member_name)
            family["members"].append(self._new_member(member_name, role))
            return _family_view(family)

    def remove_member(self, family_id: str, member_id: str,
                      operator_id=None, operator_name=None) -> dict:
        """管理员删除成员；户主不能被删（否则这个组就没人能管了）。"""
        member_id = (member_id or "").strip()
        if not member_id:
            raise FamilyError(400, "BAD_MEMBER_ID", "缺少要删除的成员编号")

        with self._lock:
            family = self._family_locked(family_id)
            self._require_admin(family, operator_id, operator_name)
            target = self._find_member(family, member_id=member_id)
            if target is None:
                raise FamilyError(404, "MEMBER_NOT_FOUND", "这个成员已经不在家庭组里了")
            if target["role"] == ROLE_OWNER:
                raise FamilyError(400, "CANNOT_REMOVE_OWNER",
                                  "户主不能被删除；要换户主请重新创建家庭组")
            family["members"].remove(target)
            return _family_view(family)

    # ---------------------------------------------------------------- 查询
    def get(self, family_id: str) -> dict:
        with self._lock:
            return _family_view(self._family_locked(family_id))

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
