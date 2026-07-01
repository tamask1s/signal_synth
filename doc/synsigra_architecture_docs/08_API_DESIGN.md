# API Design

Version: 0.1  
Status: Draft  
Scope: CLI, internal library API, SaaS HTTP API, future batch API.

## 1. API principles

The API should be:

- scenario-centric;
- deterministic;
- explicit about warnings and unsupported requests;
- safe for SaaS limits and licensing;
- usable from CI/regression workflows;
- independent from clinical claims.

## 2. Core library API

Recommended stable facade:

```cpp
namespace signal_synth {

struct ValidationMessage {
    enum class Severity { Info, Warning, Error };
    Severity severity;
    std::string code;
    std::string message;
    std::string path;
};

struct ValidationResult {
    bool ok;
    std::vector<ValidationMessage> messages;
    std::string canonical_scenario_json;
    std::string scenario_fingerprint;
};

struct RenderResult {
    std::string scenario_fingerprint;
    std::string generator_version;
    std::vector<std::string> channel_names;
    double sample_rate_hz;
    std::vector<double> samples; // or channel-major buffers
    std::string annotations_json;
    std::string metrics_json;
    std::vector<ValidationMessage> messages;
};

ValidationResult validate_scenario_json(const std::string& scenario_json);
RenderResult render_scenario_json(const std::string& scenario_json);
std::string export_csv(const RenderResult& result);
std::string export_json_package(const RenderResult& result);
std::string render_html_report(const RenderResult& result);

}
```

## 3. CLI API

Commands:

```bash
signal-synth validate scenario.json
signal-synth fingerprint scenario.json
signal-synth render scenario.json --out output/
signal-synth report scenario.json --out output/report.html
signal-synth examples --out examples/
```

Current implemented local commands are `validate`, `fingerprint`, and
`render`. `render` creates the deterministic ECG or ECG+PPG evidence package
defined in `10_EXPORT_AND_REPORTING.md`; the remaining commands above are
planned contracts.

Future commands:

```bash
signal-synth compare scenario.json detections.csv --out report.html
signal-synth batch scenarios/ --out batch_report/
signal-synth schema --version 1.0
```

## 4. HTTP API

### 4.1 Validate scenario

```http
POST /api/v1/scenarios/validate
Content-Type: application/json
```

Request body: scenario JSON.

Response:

```json
{
  "ok": true,
  "scenario_fingerprint": "sha256:...",
  "messages": [],
  "canonical_scenario": {}
}
```

### 4.2 Render scenario

```http
POST /api/v1/render
Content-Type: application/json
```

Response:

```json
{
  "job_id": "job_123",
  "status": "queued"
}
```

For small scenarios, synchronous render can be allowed initially:

```http
POST /api/v1/render:sync
```

### 4.3 Get job status

```http
GET /api/v1/jobs/{job_id}
```

Response:

```json
{
  "job_id": "job_123",
  "status": "completed",
  "result_id": "result_456",
  "messages": []
}
```

### 4.4 Download export

```http
GET /api/v1/results/{result_id}/exports/waveform.csv
GET /api/v1/results/{result_id}/exports/annotations.json
GET /api/v1/results/{result_id}/report.html
```

### 4.5 Scenario library

```http
GET /api/v1/scenario-packs
GET /api/v1/scenario-packs/{pack_id}
GET /api/v1/scenarios/templates/{template_id}
```

## 5. SaaS entity model

Core entities:

- User;
- Organization;
- Project;
- Scenario;
- RenderJob;
- RenderResult;
- ExportArtifact;
- ScenarioPack;
- LicensePlan;
- AuditEvent.

## 6. Rate limits and export limits

Limits should be productized early.

Examples:

| Plan | Max duration | Batch count | Export formats |
|---|---:|---:|---|
| Free | 30 s | 1 | CSV/JSON watermark |
| Pro | 10 min | 20 | CSV/JSON |
| Lab | 60 min | 200 | CSV/JSON/WFDB later |
| Enterprise | custom | custom | all enabled |

## 7. Error model

Use structured error codes:

```json
{
  "error": {
    "code": "SCENARIO_UNSUPPORTED_CONDITION",
    "message": "Requested condition 'complete_av_block' is not supported by selected generator.",
    "path": "$.events[2]",
    "severity": "error"
  }
}
```

Error categories:

- schema_error;
- semantic_error;
- unsupported_condition;
- incompatible_conditions;
- render_limit_exceeded;
- license_limit_exceeded;
- internal_error.

## 8. Compare API later

The platform becomes stronger when customers can compare their algorithm output.

```http
POST /api/v1/results/{result_id}/compare/rpeaks
```

Accepted input:

```json
{
  "format": "rpeak_times_seconds",
  "detections": [1.023, 1.856, 2.702]
}
```

Response:

```json
{
  "sensitivity": 0.998,
  "positive_predictive_value": 0.992,
  "mean_abs_timing_error_ms": 4.1,
  "false_positive_count": 2,
  "false_negative_count": 1
}
```

## 9. API versioning

Use URL versioning for SaaS:

```text
/api/v1/...
```

Use schema versioning for scenario:

```json
"schema_version": "1.0"
```

Use generator semantic version separately:

```json
"generator_version": "0.3.0"
```

## 10. Security requirements for API

- authentication required for persistent render jobs;
- signed URLs for downloads;
- audit log for exports;
- per-organization isolation;
- generated export watermark metadata;
- no public listing of generated datasets unless explicitly enabled internally.

## 11. Implementation order

1. CLI validate.
2. CLI render to local output directory.
3. CLI report.
4. Internal C++ facade.
5. Minimal HTTP wrapper.
6. Async job processing.
7. SaaS account/project storage.
8. Compare API.
