# Gateway users and alarm access grants

This opt-in feature separates gateway-wide identities and PINs from per-alarm
access policy. Existing upstream installations that never enroll an alarm keep
legacy `code0` behavior. Listing users does not enroll an alarm. The first grant
must establish an enabled, unrestricted owner; enrollment and that grant commit
in one transaction. Adding an alarm does not create any user grants.

This replaces the earlier experimental alarm-scoped slot API and schema. There
is no runtime compatibility mode for that experiment. An unconverted experimental
managed database fails closed rather than falling back to legacy PIN validation.
Existing experimental data requires an offline, backed-up conversion before
installing this runtime. Never infer shared identity from equal names or PINs.

## Identity and grants

A gateway identity has an immutable random `id`, display `name`, `enabled` flag,
one salted PIN hash, and `user_revision`. It has no implicit access to any alarm.
Changing its PIN, name or enabled flag affects every grant of that identity.
PINs are 4–16 ASCII digits; leading zeros are significant. PINs and hashes are
never returned by this API or included in access events.

Each alarm grant has its own `revision` and these fields:

| Field | Meaning / default |
| --- | --- |
| `grant_enabled` | Enable this grant; defaults to true |
| `owner` | Owner role for this alarm; defaults to false |
| `arm`, `disarm` | Allowed operations across its permitted routes; default true |
| `api_arm_disarm` | Permit authenticated REST commands using this PIN; default false |
| `all_keypads` | Permit all keypads assigned to this alarm, including future assignments; default false |
| `keypads` | Array of `{source, endpoint}` restrictions; default empty |
| `remaining_uses` | Shared allowance across the grant's keypads; null means unlimited |
| `schedule` | Existing schedule-v1 object, or null for unrestricted time |

An empty `keypads` list with `all_keypads:false` permits no physical keypad.
A service identity can therefore have REST access without physical access.
`all_keypads:true` requires an empty explicit list. Explicit sources use canonical
lowercase, unpadded hexadecimal IEEE addresses and endpoints 1–240. The existing
alarm/device association check still applies: a grant never assigns a device to
an alarm. Restrictions cannot bypass that check.

Every enrolled alarm must retain at least one enabled identity with an enabled
owner grant, arm and disarm permission, REST access, unlimited uses and no
schedule/expiry. Owners may have selected physical keypads or no physical access.
Protection depends on the role and the remaining owners, not a slot, name or
special immutable owner ID. Create another owner before removing the last one.

PIN uniqueness is per alarm, including disabled identities and disabled grants.
Different identities can share a PIN only when their alarm sets do not overlap.
Adding a grant and rotating a PIN check the complete affected set atomically.
An existing identity's current PIN is required when adding a new grant because
salted hashes cannot be compared directly for PIN equality. That operation
verifies the supplied PIN; it does not rotate it.

## API

All routes below are beneath `/api/<apikey>` and require ordinary authenticated
REST access. The API key is still an administrative credential; a user PIN is
not an API authentication token or a separate admin role.

| Route | Methods |
| --- | --- |
| `/alarmsystems/users` | GET global identities; POST a new identity without grants |
| `/alarmsystems/users/<uid>` | GET / PUT identity; DELETE only after all grants are removed |
| `/alarmsystems/<alarm>/users` | GET identity/grant projections; POST new identity plus its first grant |
| `/alarmsystems/<alarm>/users/<uid>` | GET / PUT grant projection; DELETE this alarm grant only |
| `/alarmsystems/<alarm>/users/capabilities` | GET capability and managed-state flags |
| `/alarmsystems/<alarm>/users/lockout` | GET / PUT policy; DELETE explicit reset |

Collections are objects keyed by immutable UID. No numeric user slots exist.
The capability response includes `global_users_version:2`, `per_alarm_grants:true`,
`max_users:256`, schedule-v1, access-event-v1, keypad-lockout-v1,
`rest_command_events:true` and `alarm_timing_version:1`.

Every write includes both `revision` and `user_revision`. For a new identity both
are zero. For a new grant, `revision` is zero and `user_revision` is the existing
identity's current revision. For global identity writes both equal its current
identity revision. Grant edits compare both revisions, so a concurrent identity
change or use consumption cannot be overwritten by stale policy edits.

PUT merges omitted fields with current metadata. `pin`, when supplied, must be a
nonempty string. A grant PUT may update global identity fields atomically with the
grant; clients must make the cross-alarm scope clear. Identity revisions advance
when identity fields change; grant revisions advance on grant writes and accepted
physical access. DELETE accepts only the two revision fields. Unknown fields,
nonintegral revisions, invalid schedules and ambiguous identities are rejected.

Example new owner enrollment (synthetic PIN; send credentials in the body only):

```json
{"revision":0,"user_revision":0,"name":"Owner","enabled":true,"pin":"1357",
 "owner":true,"api_arm_disarm":true,"all_keypads":false,"keypads":[],
 "remaining_uses":null,"schedule":null}
```

Example attaching an existing global identity to another alarm:

```json
{"revision":0,"user_revision":3,"pin":"2468","arm":false,"disarm":true,
 "api_arm_disarm":false,"all_keypads":false,"keypads":[{"source":"abc","endpoint":1}],
 "remaining_uses":5,"schedule":null}
```

Managed alarms reject legacy config `code0` writes, which cannot unambiguously
select a global identity. Use the UID-based, revision-checked API. Existing REST
arm/disarm commands retain their route and `{"code0":"..."}` body, so clients
using a saved PIN do not need a package update. Legacy unmanaged config/commands
keep their upstream behavior.

## Authorization and events

Only an accepted physical keypad DISARM consumes one use, including an already-disarmed acceptance. ARM and REST do not consume uses. Schedules, global enabled
state, grant enabled state, remaining allowance and operation permissions apply
at authorization. A grant's allowance and schedule are shared across its allowed
keypads and independent of every other alarm grant.

SQLite transactions serialize edits and final-use admission. Duplicate physical
requests within the existing ten-second window retain their original receipt
and do not consume another use. Duplicate receipts and lockout state survive
restart. Storage/schedule errors produce not-ready, never a false invalid-code
access decision. Rejected access never exposes a matched ineligible identity.

Accepted access and REST events carry immutable `user_id` without `user_slot`.
`access` events remain physical-keypad decisions. `alarm_command` remains a
separate REST event with an operation and accepted/rejected/failed outcome;
it must not be routed as physical keypad input. REST events consume no use and
do not participate in keypad lockout. They identify the credential, not the
human or automation using a third-party client. Accepted commands do not prove
an exit delay completed or any physical output operated. No event replay is
provided across connection gaps.

## Lockout, schedules and timing

Optional failed-PIN lockout remains scoped to alarm + physical source + endpoint.
A wrong PIN cannot identify a user. Default policy is disabled, threshold 6 in
60 seconds, durations `[60,600,1800]`, quiet reset 3600 seconds. These defaults apply when no policy has been saved; existing saved policies are unchanged. The threshold counts failed requests, not physical PIN-entry sessions. Durations must
be ascending, 1–3600 seconds; threshold 1–100, window 1–3600, reset 3600–604800.
PUT includes all policy fields and its revision. DELETE requires `{"reset":true}`.
Active deadlines are preserved by enabled-policy edits. Disabling or explicit
reset clears active locks and escalation. Locked attempts do not evaluate PINs
or extend the deadline. Known but ineligible PINs are rejected without counting
as unidentified wrong PINs. REST remains independent of physical lockout.

Schedules use version 1, an IANA `timezone`, UTC epoch-millisecond `not_before`/`expires_at` values
or null, and weekly `windows` with ISO weekday 1–7 and start/end minute values.
End boundaries are exclusive; split overnight windows. Empty windows impose no
weekly restriction. Production validation and timezone/DST behavior are shared
with the existing schedule implementation; malformed policies fail closed.

Alarm timing configuration remains `armed_stay_*`, `armed_night_*`, and
`armed_away_*` with `entry_delay`, `exit_delay`, and `trigger_duration` suffixes.
Values are whole seconds 0–255. Configure while disarmed and read back values;
this upstream configuration API is not an atomic compare-and-swap batch API.

## Xfinity URC4450 keypad resources

The URC4450BC0-X-R has a dedicated DDF in
`devices/xfinity/urc4450_keypad.json`. Its IAS ACE action/panel/countdown
resource retains the endpoint 1 / cluster 0x0501 unique ID, and its
temperature resource retains endpoint 1 / cluster 0x0402. IAS Zone
alarm/tamper/low-battery state and enrollment fields are separated into
a ZHAAlarm resource at endpoint 1 / cluster 0x0500, so those reports do not
share the action-bearing resource.

Clients which used tamper, low-battery, enrollment or pending fields on
the URC4450 ancillary-control resource should instead use the IAS Zone
resource. This device-resource change is independent of enrolling managed
users. Existing installations with a local custom DDF must reconcile that
override before adopting the packaged DDF. The TKA105 and H34450BA00-00007
definitions remain unchanged; the resource split has only been exercised
with URC4450BC0-X-R hardware.
