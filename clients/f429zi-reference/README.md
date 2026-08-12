# STM32F429ZI OTA Reference

Nucleo-F429ZI, ESP-01 AT firmware, Wakaama와 Device Integration Kit을
실제 EVSE_BOOT에 연결한 E2E reference다.

## 책임 흐름

```text
Spring/Leshan
  -> LwM2M /5 Package URI Write
  -> Wakaama + Kit
  -> ESP-01 CoAP Block2 download
  -> STM32 bank2 staging
  -> LwM2M /5 Update Execute 응답
  -> EVSE_BOOT가 bank1 application 교체
  -> 새 application 부팅 및 Update Result 보고
```

이 코드는 통합 가능성을 검증하는 reference다. 제품용 signature, manifest,
anti-rollback, 전원 차단 rollback과 DTLS는 아직 포함하지 않는다.

## Flash layout

| 영역 | 주소 | 용도 |
|---|---:|---|
| Bootloader | `0x08000000` | EVSE_BOOT |
| Application | `0x08020000`–`0x08100000` | 실행 이미지 |
| Staging | `0x08100000`–`0x081E0000` | 다운로드 이미지 |
| Reserved | `0x081E0000`–`0x08200000` | 향후 metadata/rollback |

Application과 staging의 최대 크기는 각각 896KiB다.

## Build

Wi-Fi 값은 git에서 제외된 `Core/Inc/wifi_credentials.local.h`에 둔다.

```c
#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."
```

기준 앱 `0.1.0`:

```bash
cmake --preset Debug
cmake --build --preset Debug -j2
```

OTA 대상 앱 `0.2.0`:

```bash
cmake -S . -B build/Update -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DF429ZI_APP_VERSION=0.2.0
cmake --build build/Update -j2
```

두 빌드는 ELF와 BIN을 함께 생성한다.

## Initial flash

저장소 root에서 실행한다.

```bash
cmake --build EVSE_BOOT/build/Debug -j2
STM32_Programmer_CLI -c port=SWD \
  -w EVSE_BOOT/build/Debug/Bootloader.elf -v
STM32_Programmer_CLI -c port=SWD \
  -w clients/f429zi-reference/build/Debug/f429zi-reference.elf -v -rst
```

정상 기준은 UART의 `version=0.1.0`과 LwM2M `READY`다.

## OTA E2E

artifact server:

```bash
cd clients/f429zi-reference
python3 tools/coap_artifact_server.py \
  build/Update/f429zi-reference.bin --port 5685
```

Package URI 전달:

```bash
curl -i -H 'Content-Type: application/json' \
  -d '{"packageUri":"coap://10.10.16.58:5685/firmware.bin"}' \
  http://localhost:8081/api/devices/stm32-f429zi-01/firmware/download
```

다운로드 후 상태는 `state=2, updateResult=0`이다. 설치 요청:

```bash
curl -i -X POST \
  http://localhost:8081/api/devices/stm32-f429zi-01/firmware/update
```

정상 결과는 Update Execute `202`, 새 앱의 `version=0.2.0`, 그리고 최초
재등록 시 `state=0, updateResult=1`이다. Update Result 복구 값은 한 번
소비되므로 이후 별도 reset에서는 다시 `0`이 된다.

SWD upload는 코어를 멈출 수 있다. readback 뒤에는 필요하면 다음으로
실행을 재개한다.

```bash
STM32_Programmer_CLI -c port=SWD -run
```
