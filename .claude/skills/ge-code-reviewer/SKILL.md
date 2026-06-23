---
name: ge-code-reviewer
description: >
  对 GitCode 上的 GE (Graph Engine) 项目 PR 进行自动化代码审查。
  自动分析代码变更，检查内存泄漏、安全漏洞和可读性，生成结构化报告并发布评论。
  当用户需要审查 GitCode PR、检视代码改动、查看 PR 质量、检查代码问题、
  对 PR 发表评论、review PR、代码走查、PR 检视、审查这个 PR、
  看看这个 PR 有什么问题、检查 PR 代码质量、给 PR 提意见、
  或者提到"代码审查"、"code review"、"review 这个 MR"、"检视这个合并请求"时使用此 skill。
  也适用于用户粘贴了 GitCode PR 链接或 PR 编号并要求审查的场景。
---

# GitCode PR 代码审查

对指定的 Pull Request 进行代码审查。

**Agent 假设（适用于所有 agent 和子 agent）：**
- 所有工具都能正常工作，不会出错。不要测试工具或进行探索性调用。确保每个启动的子 agent 都清楚这一点。
- 只有在完成任务需要时才调用工具。每次工具调用都应有明确目的。

## 代码检视检查清单（必须逐项完成）

**在执行以下步骤前，必须先完成以下检查。这些检查项不可跳过，不可用后续步骤替代。**

- [ ] **编码红线**：完整读取项目根目录下 `docs/zh/contributions/coding_red_lines.md`，逐条核对修改是否违反红线规则
- [ ] **关键特性设计原则和软件约束**：根据修改涉及的目录，加载项目根目录 AGENTS.md 中"关键特性设计原则和软件约束"中对应的架构文档，确认修改不违反已有设计约束（如内存模型、流分配规则、图拆分策略等）。不要一次性加载全部文档，避免占用过多 context，而是匹配触发词、涉及目录或场景中任意一条即加载。
- [ ] **跨特性交叉影响（cross-feature-check）**：检查本次修改是否影响其他特性的行为。必须先读取项目根目录 `docs/zh/design/cross_feature_check.md`，按其指引逐场景分析：
  1. 识别本次修改涉及的所有模块/目录
  2. 在"关键特性设计原则和软件约束"文档表中查找与这些模块相关的其他特性文档
  3. 按 cross_feature_check.md 中的场景表逐项评估适用性和影响
  4. 输出评估结论，如有风险需明确标注

**检查结果必须使用以下格式输出，不得省略或修改结构：**

```
### 代码检视检查结果
- [x] 编码红线：已检查，无违反
- [x] 关键特性设计原则：已加载 memory-constraints.md，修改符合内存复用约束
- [x] 跨特性交叉影响：已按 cross_feature_check.md 逐场景分析，
      修改涉及 compiler/graph/build/memory/，已检查 stream_allocator.md，
      确认本次内存分配改动不影响流分配逻辑
```

---

## 审查流程

请严格按照以下步骤执行：

### 步骤 1: 前置检查

检查以下条件是否成立：
- PR 已关闭
- PR 是草稿状态
- PR 不需要代码审查（如：自动化 PR、明显正确的微小变更）
- Claude 已经在此 PR 上评论过（通过 GitCode API 检查 PR 评论）

如果以上任一条件成立，停止执行，不继续后续步骤。

**注意**：仍然需要审查 Claude 生成的 PR。

### 步骤 2: 获取并列出项目规范上下文

返回所有相关规范文件的文件路径列表（不含内容）：
- 根目录的规范文件（如 CONTRIBUTING.md 等）
- 根目录 AGENTS.md、CLAUDE.md 中关于检视代码要求的内容
- PR 修改文件所在目录中的规范文件

**明确列出这些文件**

### 步骤 3: 获取 PR 变更摘要

查看 PR 并返回变更摘要：

```bash
# 获取 PR 信息
curl -s "https://gitcode.com/api/v5/repos/${owner}/${repo}/pulls/<PR_NUMBER>?access_token=$GITCODE_API_TOKEN"

# 获取 PR 变更文件列表
curl -s "https://api.gitcode.com/api/v5/repos/${owner}/${repo}/pulls/<PR_NUMBER>/files.json?access_token=$GITCODE_API_TOKEN"

# 获取 PR diff
curl -s "https://gitcode.com/api/v5/repos/${owner}/${repo}/pulls/<PR_NUMBER>?access_token=$GITCODE_API_TOKEN" \
  -d "diff=true"
```

### 步骤 3.1: 确定问题代码的准确行号

发布行内评论需要准确的行号。通过以下方法确定：

#### 方法 1: 从 PR Diff 获取

```bash
# 获取 PR 变更文件列表（包含 diff 信息）
curl -s "https://api.gitcode.com/api/v5/repos/${owner}/${repo}/pulls/<PR_NUMBER>/files.json?access_token=$GITCODE_API_TOKEN"
```

返回的 `patch.diff` 字段包含 hunk header，格式为：
```
@@ -old_start,old_count +new_start,new_count @@
```

- `-old_start,old_count`: 原文件的起始行和变更行数
- `+new_start,new_count`: 新文件的起始行和新增行数

**注意**：hunk header 只提供了起始行号，需要根据 diff 内容逐行计算实际行号。

#### 方法 2: 从 Raw 文件验证（推荐）

直接查询 PR 分支的 raw 文件获取准确行号：

```bash
# 获取 PR head commit 的 raw 文件
curl -s "https://raw.gitcode.com/${owner}/${repo}/raw/<HEAD_SHA>/path/to/file.cc" | grep -n "问题代码模式"
```

**示例**：
```bash
# 查找 if (it == 的行号
curl -s "https://raw.gitcode.com/${owner}/${repo}/raw/358192edfc1809f6fe17b0da0b1b8efd9880a52f/hcom_graph_optimizer.cc" \
  | grep -n "if (it =="
```

输出：`844:      if (it == (itMap->second).end()) {`

#### 最佳实践

1. **先用 diff 定位大致范围**：通过 PR diff 找到变更的代码块
2. **再用 raw 文件确认精确行号**：避免手动计算出错
3. **发布评论前验证**：确保行号对应的是真正的问题代码

### 步骤 4: 代码审查

从以下方面全面审查变更，返回问题列表，每个问题包含描述和标记原因（如"规范合规性"、"Bug"）：

**规范合规性审查**
检查变更是否符合项目规范。注意：评估文件的规范合规性时，只考虑该文件路径或父目录中的规范文件。

**Bug 扫描**
扫描明显的 Bug。只关注 diff 本身，不读取额外上下文。只标记显著的 Bug；忽略细微问题和可能的误报。不要标记需要查看 diff 之外上下文才能验证的问题。

**代码问题检查**
查找引入代码中的问题。可能是安全问题、逻辑错误等。只查找变更代码范围内的问题。

**关键原则：只需要高信号问题。** 标记以下类型的问题：
- 代码无法编译或解析（语法错误、类型错误、缺少导入、未解析引用）
- 无论输入如何，代码肯定会产生错误结果（明显的逻辑错误）
- 明确、无歧义的规范违反，可以引用被违反的具体规则

**不要标记：**
- 代码风格或质量问题
- 依赖于特定输入或状态的潜在问题
- 主观建议或改进意见

**如果你不确定某个问题是否真实存在，不要标记它。** 误报会损害信任并浪费审查者时间。

此外，每个子 agent 都应被告知 PR 标题和描述。这有助于理解作者的意图。

### 步骤 5: 验证问题

对于步骤 4 发现的每个问题，进行验证。验证工作应获取 PR 标题和描述以及问题描述，确信地验证所述问题确实是真实问题。

例如，如果标记了"变量未定义"这样的问题，验证代码中确实如此。另一个例子是规范合规问题，验证被违反的规范规则确实适用于此文件且确实被违反。

### 步骤 6: 过滤问题

过滤掉步骤 5 中未验证通过的问题。这一步将得到我们的高信号问题列表。

### 步骤 7: 输出审查摘要

在终端输出审查结果摘要：
- 如果发现问题：列出每个问题及简要描述
- 如果未发现问题：输出"未发现问题。已检查 Bug 和规范合规性。"

如果 **未提供** `--comment` 参数，在此停止。不要发布任何 GitCode 评论。

如果 **提供了** `--comment` 参数且 **未发现问题**，使用 GitCode API 发布摘要评论并停止。

如果 **提供了** `--comment` 参数且 **发现问题**，继续步骤 8。

### 步骤 8: 准备评论列表

创建计划发布的所有评论列表。这仅供你自己确认对评论内容满意。不要在任何地方发布此列表。

### 步骤 9: 发布行内评论

使用 GitCode API 为每个问题发布行内评论。

#### 创建新的 Discussion（推荐）

使用 v5 API 提交行内评论：

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  "https://api.gitcode.com/api/v5/repos/${owner}/${repo}/pulls/<PR_NUMBER>/comments?access_token=$GITCODE_API_TOKEN" \
  -d '{
    "body": "评论内容",
    "path": "文件相对路径",
    "position": <结束行号>,
    "start_position": <起始行号>
  }'
```

**参数说明**：

| 参数 | 说明 | 必需 |
|------|------|------|
| `body` | 评论内容 | ✅ |
| `path` | 文件相对路径 | ✅ |
| `position` | 结束行号 | ✅ |
| `start_position` | 起始行号（多行选择） | 多行时 |

对于每个评论：
- 提供问题的简要描述
- 对于小型、自包含的修复，包含可提交的建议代码块
- 对于较大的修复（6+ 行、结构性变更或跨多个位置的变更），描述问题和建议修复方式，不包含建议代码块
- 永远不要发布可提交建议，除非提交该建议能完全修复问题。如果需要后续步骤，不要留下可提交建议。

**重要：每个唯一问题只发布一条行内评论。不要发布重复评论。**

## 误报列表

在步骤 4 和 5 评估问题时使用此列表（这些是误报，不要标记）：

- 已存在的问题
- 看起来是 Bug 但实际上是正确的代码
- 高级工程师不会标记的吹毛求疵
- Linter 会捕获的问题（不要运行 Linter 验证）
- 一般代码质量问题（如缺乏测试覆盖、一般安全问题），除非规范中明确要求
- 规范中提到但在代码中明确静默的问题（如通过 lint ignore 注释）

## 注意事项

- 使用 GitCode API 与 GitCode 交互（如获取 Pull Request、创建评论）。不要使用网页抓取。
- 开始前创建待办列表。
- 必须在行内评论中引用并链接每个问题（如引用规范文件时，包含指向它的链接）。
- 如果未发现问题且提供了 `--comment` 参数，发布以下格式的评论：

---

## 代码审查

未发现问题。已检查 Bug 和规范合规性。

---

- 在行内评论中链接代码时，严格遵循以下格式，否则 Markdown 预览无法正确渲染：
  `https://gitcode.com/{owner}/{repo}/blob/{full_git_sha}/path/to/file#L{start}-L{end}`
  - 需要完整的 git sha
  - 必须提供完整 sha。类似 `https://gitcode.com/owner/repo/blob/$(git rev-parse HEAD)/foo/bar` 的命令不起作用，因为评论会直接在 Markdown 中渲染
  - 仓库名必须与你正在代码审查的仓库匹配
  - 文件名后使用 # 号
  - 行范围格式为 L[start]-L[end]
  - 在你评论的行前后至少提供 1 行上下文（如评论第 5-6 行，应链接到 L4-L7）

## GitCode API 参考

**重要**：GitCode 使用 API v5 格式。

**认证方式**：
- 推荐使用 query 参数：`?access_token=$GITCODE_API_TOKEN`
- 也可使用 Header：`PRIVATE-TOKEN: $GITCODE_API_TOKEN`

### 快速参考

| 操作 | API 端点 |
|------|---------|
| 获取 PR 信息 | `GET /repos/${owner}/${repo}/pulls/<PR_NUMBER>` |
| 获取 PR 变更 | `GET /repos/${owner}/${repo}/pulls/<PR_NUMBER>/files.json` |
| 获取 PR 评论 | `GET /repos/${owner}/${repo}/pulls/<PR_NUMBER>/comments` |
| 发布普通评论 | `POST /repos/${owner}/${repo}/pulls/<PR_NUMBER>/comments` |
| 发布行内评论 | `POST /repos/${owner}/${repo}/pulls/<PR_NUMBER>/comments` |

### 基本格式

```bash
curl -s "https://api.gitcode.com/api/v5/repos/${owner}/${repo}/pulls/<PR_NUMBER>/comments?access_token=$GITCODE_API_TOKEN"
```

详细 API 参数、响应码和权限说明请参考 `references/gitcode_api.md` 的「删除 PR 评论」章节。