# libsmap1 Public API — 功能范围

本文档约定 libsmap1 v1 对外提供的功能边界.
具体类 / 函数签名以 `include/smap1/` 下的 Doxygen 注释为准.

## 分层

- **Transport** (`smap1::proto::*`) — protobuf 生成类, 是地图在内存 / wire / 文件 / 前端通信中的权威表示.  调用方直接用 protobuf API 做序列化 (binary 或 JSON), libsmap1 不再包装这一层.
- **Codec** (`smap1::codec::*`) — 原始地图格式 (rbk34 单文件、rbk35 文件夹) 与 `proto::MapPackage` 之间的双向转换.  直接操作 proto, 不依赖 wrap 层.
- **Wrap** (`smap1::Map`) — 拥有一个 `proto::MapPackage`, 提供常用查询 / 编辑的便利方法.  通过 `.pb()` 暴露底层 proto, 允许调用方直接读写 proto 字段.  不持索引缓存, 也不对底层 proto 做不可变假设.
- **Geometry** (`smap1::geometry::*`) — 构造 `proto::CurveGeometry` 的自由函数.
- **Validation** — 独立函数, 检查 `proto::MapPackage` 内部一致性.

## Codec

- `load(path, format)` — 从原始格式加载为 IR.  `path` 为 `std::filesystem::path`; `format` 是显式枚举 (`Rbk34` 指向单 `.smap` 文件, `Rbk35` 指向文件夹).  不做自动判定.
- `save(package, path, format, options)` — 将 IR 写回原始格式.  `options` 中 `overwrite` 默认 false, 目标已存在且非空时返回错误.
- `load_robot_model(path, format)` — 从 robot model 文件加载为 `proto::RobotModel`.  `format` 是 `RobotModelFormat` 枚举 (`Rbk34` / `Rbk35`).  robot model 只读, 因此只提供 load 不提供 save.  关键字段 (chassis / shape) 缺失时返回 `InvalidArgument`.
- 保真: encode 时尽量利用 `SourceBundle` / `RawPayload` / `Property.legacy_value` 做无损回写.

## Wrap (`smap1::Map`)

构造:
- 只接受 `proto::MapPackage`, 调用方可传入左值 (复制) 或右值 (move); 内部一律 move 进成员.
- 不默认构造、不拷贝构造.  构造时不做校验.

访问底层 proto:
- `pb() const&` / `pb() &` — 返回 `proto::MapPackage` 引用.
- `into_pb() &&` — 移出底层 proto, Map 之后不可再用.

Station (v1):
- `station_count()`.
- `find_station(id)` — 返回 `proto::Station*` (找不到返回 nullptr); const 版返回 `const proto::Station*`.
- `add_station(proto::Station)` — 校验 ID 唯一; 返回 `expected<proto::Station*, Error>`.
- `remove_station(id)` — 级联删除所有引用此站点的 Path; 返回 bool 指示是否真删除.

Path (v1):
- `path_count()`.
- `find_path(id)` / 可变版.
- `find_paths_between(start_id, end_id)` — 返回所有匹配 Path 的指针向量.
- `add_path(proto::Path)` — 校验 ID 唯一 + 起止站点存在 + `CurveGeometry` 首/尾控制点匹配站点 pose; 返回 `expected<proto::Path*, Error>`.
- `remove_path(id)` — 返回 bool.

修改字段:
- 不提供 setter.  调用方通过 `find_*` 拿到 `proto::T*` 后直接调 protobuf 生成的 setter / `mutable_*`.

约束:
- Path 的起止端点必须是 Station.  站点不可移动 (仅允许 add / remove); 删除站点时级联删 Path, 因此不会出现端点失效的悬挂状态.
- `CurveGeometry` 仍保存包含首尾的完整控制点; 首/尾必须与对应站点的 pose 一致, 由 `add_path` 校验.

Robot model (v1):
- `set_robot_model(proto::RobotModel)` — 关联一个 robot model.  按值传入 (左值复制 / 右值 move), Map 内部按值持有, 不引用原对象.
- `clear_robot_model()` — 解除当前关联.
- `robot_model()` — 返回当前关联的 model 指针 (未设置返回 nullptr); 指向 Map 内部的副本, 寿命与 Map 一致.
- `footprint_at(pose, samples)` — 把机身轮廓 (rectangle / circle / polygon) 经 `shape_to_chassis` 与 `pose` 变换到 map 坐标系, 返回 `proto::Polygon`.  圆形按 `samples` 个点近似 (默认 32).  未设置 model 时返回空 Polygon.
- `check_in_bounds(footprint)` — 判断 footprint 是否完整落在 `header.bounds` 之内.  bounds 缺失或退化时一律返回 true (不做空间约束).
- 已设置 robot model 且地图有可用 bounds 时, `add_station` 会顺带做越界校验, footprint 出界返回 `Error::Code::OutOfBounds`.  不会自动复检已有站点.

## Geometry (`smap1::geometry`)

- `make_line_segment(start, end)` — 构造 `kind = LINE_SEGMENT` 的 `CurveGeometry`.
- `make_bezier(control_points)` — 构造 `kind = BEZIER` 的 `CurveGeometry`, `degree = control_points.size() - 1`.  退化情况 (如 5 阶 Bezier 中间 4 个控制点两两重合) 直接用重复点表示.

这两个是纯构造函数, 不做语义校验; 不合法输入会得到一个无效的 `CurveGeometry`, 但库本身不会崩.

## Validation

独立函数, 输入 `const proto::MapPackage&` 或 `const Map&`, 输出 issue 列表.
检查项:

- ID 唯一性 (Station, Path).
- Path 起止站点存在.
- Path `CurveGeometry` 首/尾控制点匹配站点 pose.
- `CurveGeometry` 控制点数与 `degree` 一致 (Bezier).

调用方决定如何对待 issue (报错 / 警告 / 忽略).

## Error 处理

可失败的 API 一律返回 `std::expected<T, Error>`:

```
struct Error {
    enum class Code { /* FileNotFound, ParseFailed, IdConflict, IdNotFound, EndpointMismatch, ... */ };
    Code code;
    std::string message;
};
```

- `code` 用于编程判定, `message` 用于日志 / 报错展示.
- 整库共用一个 `Code` 枚举.
- 不携带 `std::source_location` / 调用栈.

## 显式不做

以下功能在 v1 不提供, 调用方需要时自行实现或等后续 milestone:

- IR ↔ protobuf/JSON 字符串的序列化封装 (直接用 protobuf API).
- 从内存字节加载原始格式 (只走 `std::filesystem::path`).
- 自动判定原始格式类型 (`SourceFormat` 必须显式传).
- Station / Path 之外的业务对象 CRUD helpers (FeatureLine, Area, Reflector, TagGroup, BinLocation, Device, Charger, Policy, Route, BinTask 等通过 `.pb()` 直接操作 proto).
- Bezier 求值 / 采样.
- 索引缓存 (一律线性扫描; 性能需要时再加).
- 站点位姿修改 (仅允许 add / remove).
- 撤销 / 事务.
