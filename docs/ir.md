# 地图 IR

`proto/smap1/ir.proto` 定义了 smap1 的地图中间表示（IR）。IR 的目标是在不同原始地图格式之间建立一个可编辑、可扩展、尽量可无损回写的公共结构。

当前约定的原始格式标识为：

- `rbk34`：RBK v3.4 单文件 JSON 地图。
- `rbk35`：RBK v3.5 文件夹地图，包含 `*.smap`、`*.2dft`、`*.2dlh`、`*.2dpc` 等文件。

## 设计原则

- 语义字段优先归一化，便于编辑器和转换器直接修改地图对象。
- 原始文件、原始记录和未知字段必须尽量保留，避免未编辑内容在回写时丢失。
- 用户数据、扩展属性和源格式中可能有顺序或重复键的数据不使用 `map<K,V>`。
- 归一化语义字段默认使用 SI 单位；角度默认使用弧度。
- `optional` 数值字段用于区分“未设置”和“设置为 0”。

## 顶层结构

IR 的根对象是 `MapPackage`：

- `ir_schema_version`：IR schema 版本，用于兼容性判断。
- `source_format`：原始地图格式，例如 `rbk34` 或 `rbk35`。
- `map`：归一化后的可编辑地图主体。
- `source`：原始来源数据集合，用于追踪和回写。

`Map` 是主要语义对象容器，包含地图元信息、坐标系、图层、站点、路径、区域、设备、充电桩、策略、路线等内容。顶层 `properties` 和 `source_refs` 用于保存地图级扩展属性和来源引用。

## 基础几何

基础几何类型包括：

- `Point3`：三维点或向量。
- `Quaternion`：三维姿态四元数。
- `Pose2D`：二维位姿，包含位置、航向角和坐标系。
- `Pose3D`：三维位姿，包含位置、四元数和坐标系。
- `Bounds`：三维包围盒。
- `Polygon`、`LineSegment`、`CurveGeometry`：区域、线段和路径曲线。

`CurveGeometry` 用统一结构表达直线、折线和 Bezier 曲线。Bezier 曲线保存完整控制点，包括起点和终点；退化控制点用重复点直接表示。

## 图层

`Layer` 表示地图中的几何、栅格或点云图层，使用 `oneof data` 存放具体数据：

- `PointSet`：展开点集或紧凑编码点集。
- `LineSet`：特征线集合。
- `TileSet`：瓦片集合。
- `RawPayload`：尚未解析或必须原样保留的原始载荷。

`Tile` 用于 RBK v3.5 的瓦片类文件。`RbkTileHeader` 保存瓦片文件头；`payload` 可表示特征线、似然图、点云块或原始载荷。

## 业务对象

IR 将常见地图元素建模为一等对象：

- `Station`：站点或点位。
- `Path`：站点之间的路径，引用起止站点、曲线几何、策略和通行方向。
- `FeatureLine`：独立特征线。
- `Area`：区域对象。
- `Reflector`：反光板或反光柱。
- `TagGroup` / `Tag`：二维码、RFID 等定位标签。
- `BinLocation` / `BinGroup` / `BinTask`：库位、货位组和货位任务。
- `Device`：地图设备。
- `Charger`：充电桩。
- `Route`：由站点序列和运动约束组成的路线。

这些对象都保留 `properties` 和 `source_refs`，用于承载源格式中的扩展属性和来源位置。

## 策略

`Policy` 表示可复用行为或配置。RBK v3.5 中，这类配置通常是顶层 JSON 对象，并由路径等对象通过属性引用。

`Policy` 同时保存两类信息：

- 归一化字段，例如 `PathPolicy`、`LocalizationPolicy`、`MotionLimits`。
- 完整原始值，例如 `value` 和 `raw_json_text`。

这样可以让编辑器修改常用语义字段，同时让转换器在未理解全部配置时仍保留原始内容。

## 扩展与保真

IR 使用以下结构保留暂未归一化的数据：

- `Property` / `PropertyValue`：用户自定义属性或格式特有属性。
- `GenericObject`：暂未建模为一等对象的源格式对象。
- `RawPayload`：未解析的 protobuf wire、JSON 文本、JSON 值或未知字段。
- `UnknownField`：未识别字段的名称、编号和原始表示。

转换器应优先填充明确理解的归一化字段；对无法归一化但需要回写的数据，应放入上述扩展结构。

## 来源追踪

`SourceBundle` 保存原始来源数据：

- `SourceFile`：原始文件路径、角色、媒体类型、内容、校验和及解析辅助信息。
- `SourceSchema`：源格式 schema 或 descriptor。
- `SourceRef`：从 IR 对象回指原始文件、JSON Pointer、protobuf 类型、字段路径或原始记录。

来源追踪的目标是支持 `raw -> IR -> raw` 的尽量无损转换。语义编辑发生后，转换器应以归一化字段为准，并结合 `source_refs` 和原始载荷决定如何更新或重建目标格式。

## 兼容性约定

- 新增字段时使用新的字段编号，不复用已删除编号。
- 枚举值 `0` 保留为 `UNSPECIFIED`，无法归类但需保留的值优先落到 `OTHER` 或原始字符串字段。
- 原始格式新增对象时，可先使用 `GenericObject`、`Property` 或 `RawPayload` 承载，再逐步提升为一等对象。
