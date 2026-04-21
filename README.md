# CSV Snapshot Logger

Linux 환경에서 동작하는 **경량 주기 샘플링 로거**이다.\
외부에서 제공되는 값(item)을 일정 주기로 수집하여 CSV 파일 형태로
저장한다.

이 로거는 이벤트 발생 시마다 기록하는 방식이 아니라 **주기적으로 현재
상태(snapshot)를 기록하는 방식**이다.

또한 필요 시 사용자가 명시적으로 기록을 수행할 수 있는 **manual write
기능**을 제공한다.

------------------------------------------------------------------------

## Features

-   Linux 전용
-   CSV 파일 저장
-   주기 기반 기록 (AUTO)
-   사용자 호출 기반 기록 (MANUAL)
-   pause / resume 지원
-   파일 rotation 없음
-   파일 시작 시 header 기록
-   고정 컬럼 제공
-   auto / manual item 구분 가능
-   thread-safe 값 갱신 가능
-   낮은 부하 구조
-   단순하고 확장 가능한 구조

------------------------------------------------------------------------

## CSV Format

### Column Structure

uptime,system_time,record_type,<item1>,<item2>,...

### Fixed Columns

| column | description |
|---|---|
| uptime | Linux `/proc/uptime` 값 |
| system_time | 현재 시스템 시간 |
| record_type | AUTO 또는 MANUAL |


### Item Recording Rule

|  item kind  |  AUTO row  | MANUAL row |
|---|---|---|
  Auto | 기록됨 | snapshot 값 기록
  Manual | 빈칸 | 기록됨

manual item 값은 MANUAL write 성공 시 자동으로 clear 된다.

### Example

```csv
# device=LECU-A
# build=2026.04

uptime,system_time,record_type,speed,temperature,mode,operator_note
341011.29,2026-04-21 14:59:11,MANUAL,0,40,INIT,value1
341011.29,2026-04-21 14:59:11,MANUAL,0,40,INIT,value2
341011.29,2026-04-21 14:59:11,AUTO,0,40,INIT,value3
341012.29,2026-04-21 14:59:12,AUTO,10,41,INIT,
341012.69,2026-04-21 14:59:13,MANUAL,20,42,RUN,value4
341013.29,2026-04-21 14:59:14,AUTO,20,42,RUN,
341014.09,2026-04-21 14:59:14,MANUAL,40,44,RUN,value5
341014.29,2026-04-21 14:59:15,AUTO,40,44,RUN,
```

------------------------------------------------------------------------

## Workflow
```text
Create CsvLogger
        ↓ 
Register items
        ↓
start()
        ↓
Periodic AUTO row generation
        ↓
optional pause()
        ↓
optional write() for MANUAL row
        ↓
resume()
        ↓
stop()
```

------------------------------------------------------------------------

## Class Diagram

``` mermaid
classDiagram

    class CsvLogger {
        + addItem()
        + start()
        + stop()
        + pause()
        + resume()
        + write()
    }

    class CsvItemKind {
        <<enumeration>>
        Auto
        Manual
    }

    class CsvItemEntry {
        + item
        + kind
    }

    class CsvLoggerConfig {
        + filePath
        + headerText
        + intervalSec
        + append
    }

    class ICsvItem {
        <<interface>>
        + title()
        + valueAsString()
        + clear()
    }

    class CsvStringItem {
        + setValue()
    }

    class CsvAtomicIntItem {
        + setValue()
    }

    class CsvFileWriter {
        + open()
        + writeHeader()
        + writeRow()
        + close()
    }

    ICsvItem <|-- CsvStringItem
    ICsvItem <|-- CsvAtomicIntItem

    CsvLogger --> CsvItemEntry
    CsvItemEntry --> ICsvItem
    CsvItemEntry --> CsvItemKind

    CsvLogger --> CsvFileWriter
    CsvLogger --> CsvLoggerConfig
```
