# Documentation Review & Standardization Plan

**Ngày tạo:** 13 Tháng 1, 2026  
**Mục đích:** Chuẩn hóa và đồng bộ toàn bộ tài liệu trong `docs/` với 2 tài liệu chuẩn:
1. `emic_lora_protocol_frame_spec.md` (Specification)
2. `emic_lora_stack_architecture.md` (Architecture)

---

## 📊 Tổng Quan Hiện Tại

### Current Documentation Files

| # | File | Status | Purpose | Size | Last Update? |
|---|------|--------|---------|------|--------------|
| 1 | `emic_lora_protocol_frame_spec.md` | ✅ **CHUẨN** | Protocol specification (V2, Vietnamese) | ~500 lines | 13/1/2026 |
| 2 | `emic_lora_stack_architecture.md` | ✅ **CHUẨN** | Layer architecture & design rationale | ~700 lines | 13/1/2026 |
| 3 | `layer_call_flow.md` | ⚠️ PARTIAL | Runtime layer interactions (dependencies) | ~366 lines | 2025 |
| 4 | `main_runtime_flow.md` | ⚠️ OUTDATED | Boot + main loop flow (uses old terminology) | ~532 lines | 2025 |
| 5 | `node_smoke_layers_and_folders.md` | ⚠️ PARTIAL | Folder structure + layer proposal | ~212 lines | 2025 |
| 6 | `lora_fire_node_star_spec.md` | ⚠️ OUTDATED | System architecture + RF parameters | ~363 lines | 2025 |
| 7 | `project_tree_mapping.md` | ✓ REFERENCE | Tree structure mapping | ~50 lines | 2025 |
| 8 | `emic_lora_protocol_frame_spec_en.md` | 📦 BACKUP | English backup of frame spec | ~400 lines | 13/1/2026 |

---

## 📋 Phân Tích Chi Tiết

### File 1: ✅ `emic_lora_protocol_frame_spec.md`

**Status:** ✅ GOLDEN STANDARD (Mới nhất, V2, Tiếng Việt)

**Nội dung:**
- Frame format & header fields (Mục 5-7)
- Security (AES-128-CCM, nonce, anti-replay window=1) — Mục 8
- Message types (19 types) — Mục 9
- Bidirectional ACK mechanism — Mục 10.2
- Implementation checklist — Mục 15

**Khuyến nghị:**
- ✅ Giữ nguyên (đây là source of truth cho protocol)
- ✅ Tham chiếu từ các tài liệu khác

---

### File 2: ✅ `emic_lora_stack_architecture.md`

**Status:** ✅ GOLDEN STANDARD (Mới nhất, định nghĩa terminology)

**Nội dung:**
- PHY/MAC/Protocol layer responsibilities (Mục 2-4)
- Comparison with other standards (LoRaWAN, Zigbee, BLE Mesh, Z-Wave) — Mục 5
- Encryption vs CRC vs MIC distinctions — Mục 6
- Anti-patterns & mistakes — Mục 8
- Terminology reference (Việt ↔ English) — Mục 10

**Khuyến nghị:**
- ✅ Giữ nguyên (đây là source of truth cho architecture)
- ✅ Tham chiếu từ các tài liệu khác
- ✅ Định nghĩa terminology chính thức: **"MAC Layer"** (không "Link Layer")

---

### File 3: ⚠️ `layer_call_flow.md`

**Status:** ⚠️ PARTIAL (CẦN UPDATE: Terminology + Phần bảo mật)

**Nội dung hiện tại:**
- Dependency diagram (Layer 7→1)
- Function call chains (AI gọi ai)
- Dependency rules

**Vấn đề:**
- ❌ Sử dụng tên "link" chứ không "MAC" (inconsistent)
- ❌ Diagram không đề cập bảo mật/encryption/anti-replay
- ❌ Không reference tới `emic_lora_stack_architecture.md`

**Khuyến nghị:**
- 🔧 **UPDATE** (Priority: **MEDIUM**)
  - [ ] Cập nhật terminology: "link" → "MAC"
  - [ ] Thêm reference tới `emic_lora_stack_architecture.md` (Mục 7)
  - [ ] Thêm ghi chú: anti-replay checks ở Protocol layer
  - [ ] Update file codeflow diagram để rõ ràng hơn

---

### File 4: ⚠️ `main_runtime_flow.md`

**Status:** ⚠️ OUTDATED (Boot flow, RX/TX sequencing — cần review)

**Nội dung hiện tại:**
- Tham số cấu hình chính (RTC tick 0.5s, CAD scan 5.0s)
- Boot flow (initialization sequence)
- Main loop (state machines, event polling)
- RX/TX flow
- Remote alarm flow
- Local alarm flow

**Vấn đề:**
- ❌ Dùng tên "Link layer" (inconsistent với 2 tài liệu chuẩn)
- ❌ Không đề cập AES-128-CCM, chỉ nhắc "AES-ECB" (outdated)
- ❌ Anti-replay discussion không rõ (window=1, msg_id tracking)
- ❌ ACK mechanism description có thể cũ (không mention bidirectional)
- ⚠️ Dependency diagram tham chiếu code cũ có thể

**Khuyến nghị:**
- 🔧 **REVIEW + UPDATE** (Priority: **HIGH**)
  - [ ] Cập nhật terminology: "link" → "MAC"
  - [ ] Cập nhật encryption: "AES-ECB + CRC16" → "AES-128-CCM"
  - [ ] Cập nhật ACK mechanism: thêm bidirectional explanation
  - [ ] Cập nhật anti-replay: window=1 strictly monotonic
  - [ ] Thêm reference tới `emic_lora_stack_architecture.md` (Mục 3-4)

---

### File 5: ⚠️ `node_smoke_layers_and_folders.md`

**Status:** ⚠️ PARTIAL (Folder structure proposal — có giá trị nhưng chưa implemented)

**Nội dung hiện tại:**
- Review cấu trúc hiện tại (src/, HardwareDebug/, trash/)
- Mục tiêu chia layer cho smoke node
- Đề xuất layer stack (1–6)
- Services (domain services)

**Vấn đề:**
- ⚠️ Là **proposal**, không phải implementation
- ❌ Dùng tên "link" & cấu trúc chưa được chuẩn hóa
- ✓ Nội dung hữu ích cho future refactoring

**Khuyến nghị:**
- 📝 **REFERENCE ONLY** (Priority: **LOW**)
  - [ ] Cập nhật terminology: "link" → "MAC"
  - [ ] Thêm note: "This is a proposal for future refactoring"
  - [ ] Reference tới `emic_lora_stack_architecture.md` (Mục 7.1)
  - [ ] Thêm cross-reference tới `project_tree_mapping.md`

---

### File 6: ⚠️ `lora_fire_node_star_spec.md`

**Status:** ⚠️ OUTDATED (System spec + RF params — cần verify với V2)

**Nội dung hiện tại:**
- Scope (30 nodes, 4–5 tầng, pin 2600mAh, heartbeat 4 min, downlink ≤6s)
- System architecture (Node + Gateway)
- RF parameters (SF7, BW125, 14dBm)
- Downlink paging parameters (preamble=8, CAD=4, Tscan=5.0s)

**Vấn đề:**
- ⚠️ Dùng tên "downlink" không consistent với bidirectional ACK
- ❌ Không mention AES-128-CCM (chỉ nói "frame/message")
- ❌ Anti-replay strategy không nêu
- ⚠️ Cần verify: "Downlink paging" có phù hợp với V2 anti-replay window=1 không?

**Khuyến nghị:**
- 🔧 **REVIEW + UPDATE** (Priority: **MEDIUM**)
  - [ ] Verify: Downlink paging flow consistency với V2
  - [ ] Thêm reference tới `emic_lora_protocol_frame_spec.md` (Mục 9-10)
  - [ ] Thêm reference tới `emic_lora_stack_architecture.md` (Mục 5-6)
  - [ ] Cập nhật ACK mechanism: bidirectional (not just downlink)
  - [ ] Thêm ghi chú bảo mật: AES-128-CCM + window=1 anti-replay

---

### File 7: ✓ `project_tree_mapping.md`

**Status:** ✓ OK (Reference file, structure mapping)

**Nội dung:**
- Hiện tại implement gì
- Tại sao proposal lớn hơn hiện tại
- Future split suggestions

**Khuyến nghị:**
- ✅ Giữ nguyên (chỉ là reference)
- ✓ Có thể thêm note nhỏ reference tới 2 tài liệu chuẩn

---

### File 8: 📦 `emic_lora_protocol_frame_spec_en.md`

**Status:** 📦 BACKUP (Cấu hình dự phòng, tiếng Anh)

**Khuyến nghị:**
- ✅ Giữ nguyên (backup reference)
- ⚠️ Không thêm vào danh sách "chính thức"
- 📝 Thêm note trong README hoặc index: "English backup only"

---

## 🎯 Action Plan (Ưu Tiên)

### **Priority 1: HIGH** (Cần làm ngay)

| Task | File | Action | Effort |
|------|------|--------|--------|
| 1a | `main_runtime_flow.md` | Update terminology + encryption + anti-replay | 2–3 giờ |
| 1b | `lora_fire_node_star_spec.md` | Verify V2 compatibility + update references | 1–2 giờ |

### **Priority 2: MEDIUM** (Nên làm)

| Task | File | Action | Effort |
|------|------|--------|--------|
| 2a | `layer_call_flow.md` | Update terminology + add security context | 1–2 giờ |
| 2b | `node_smoke_layers_and_folders.md` | Update terminology + mark as proposal | 30 min |

### **Priority 3: LOW** (Nice to have)

| Task | File | Action | Effort |
|------|------|--------|--------|
| 3a | `project_tree_mapping.md` | Add reference links | 15 min |
| 3b | Create `DOCS_INDEX.md` | Central index + guidelines | 1 giờ |

---

## 📌 Terminology Standardization (Cross-All Files)

### Official Terminology (từ 2 tài liệu chuẩn)

✅ **Chính thức:**
- "**MAC Layer**" (not "Link Layer")
- "**Protocol Layer**" (for encryption/anti-replay)
- "**PHY Layer**" (for SX1262/LoRa modulation)
- "**AES-128-CCM**" (not "AES-ECB")
- "**msg_id**" (24-bit counter, Protocol layer)
- "**MAC seq**" (link-level sequence, MAC layer)
- "**Bidirectional ACK**" (GW↔ED both directions)
- "**window=1**" (strictly monotonic anti-replay)

❌ **Deprecated:**
- ~~"Link Layer"~~ → Use "MAC Layer"
- ~~"AES-ECB + CRC16"~~ → Use "AES-128-CCM"
- ~~"link seq"~~ → Use "MAC seq"
- ~~"downlink ACK only"~~ → Use "bidirectional ACK"

### Recommended Checklist for Each File

```markdown
# Standardization Checklist

- [ ] All occurrences of "Link Layer" replaced with "MAC Layer"
- [ ] All occurrences of "AES-ECB" replaced with "AES-128-CCM"
- [ ] All ACK descriptions mention "bidirectional (GW↔ED)"
- [ ] Anti-replay explanation includes "window=1, strictly monotonic"
- [ ] References added to both standard documents:
  - [ ] `emic_lora_protocol_frame_spec.md` (Mục X)
  - [ ] `emic_lora_stack_architecture.md` (Mục Y)
- [ ] No contradictions with standard definitions
```

---

## 🔗 Cross-Reference Map

### Recommended Internal References

**From any file to:**

| Ref to File | Relevant Sections | Use Case |
|-------------|-------------------|----------|
| `emic_lora_protocol_frame_spec.md` | Mục 5-7 | Frame format details |
| `emic_lora_protocol_frame_spec.md` | Mục 8 | Security (AES-CCM, anti-replay) |
| `emic_lora_protocol_frame_spec.md` | Mục 9-10 | Message types & ACK |
| `emic_lora_stack_architecture.md` | Mục 2-4 | Layer responsibilities |
| `emic_lora_stack_architecture.md` | Mục 5 | Standards comparison |
| `emic_lora_stack_architecture.md` | Mục 6 | Encryption vs CRC vs MIC |
| `emic_lora_stack_architecture.md` | Mục 7 | Code file mapping |

---

## 📐 Proposed File Structure (After Standardization)

```
docs/
├── [STANDARD DOCS]
│   ├── emic_lora_protocol_frame_spec.md ........... Protocol specification (V2, Vietnamese)
│   └── emic_lora_stack_architecture.md ........... Stack design & rationale
│
├── [IMPLEMENTATION DOCS - Updated]
│   ├── main_runtime_flow.md ...................... Boot + main loop (UPDATED: V2 terminology)
│   ├── layer_call_flow.md ....................... Runtime layer interactions (UPDATED: terminology)
│   ├── lora_fire_node_star_spec.md .............. System spec (UPDATED: V2 verified)
│   └── node_smoke_layers_and_folders.md ......... Folder proposal (UPDATED: terminology + marked as proposal)
│
├── [REFERENCE DOCS]
│   ├── project_tree_mapping.md .................. Tree structure mapping
│   └── emic_lora_protocol_frame_spec_en.md ...... English backup (reference only)
│
└── [MAINTENANCE]
    ├── DOCS_INDEX.md ............................. Central index (OPTIONAL)
    └── DOCS_REVIEW_AND_STANDARDIZATION.md ....... This file (checklist & plan)
```

---

## ✅ Next Steps

### Step 1: Approval & Planning
- [ ] Review this document với team
- [ ] Xác nhận priority order
- [ ] Assign owner cho mỗi update task

### Step 2: Execute Updates (by Priority)
- [ ] **Priority 1 Tasks** (main_runtime_flow.md, lora_fire_node_star_spec.md)
- [ ] **Priority 2 Tasks** (layer_call_flow.md, node_smoke_layers_and_folders.md)
- [ ] **Priority 3 Tasks** (project_tree_mapping.md, optional DOCS_INDEX.md)

### Step 3: Validation
- [ ] Run grep search to verify no old terminology remains
- [ ] Cross-check all reference links
- [ ] Build/compile to ensure no broken links

### Step 4: Commit
- [ ] Commit all updated files với message: "docs: standardize terminology & references (MAC layer, AES-128-CCM, bidirectional ACK, window=1)"
- [ ] Tag commit as `docs/v2-standardization` (optional)

---

## 📝 Notes

- **Scope**: Chuẩn hóa **tài liệu**, không phải **code** (code refactoring separate task)
- **Language**: Vietnamese primary (with English technical terms)
- **References**: Always reference section number (Mục X) or link number (#L10-L20)
- **Backwards Compatibility**: Keep old files as reference (e.g., frame_spec_en.md), but mark clearly

---

**Status:** Draft (waiting for team approval)  
**Last Updated:** 13/1/2026  
**Owner:** EMIC Protocol Team
