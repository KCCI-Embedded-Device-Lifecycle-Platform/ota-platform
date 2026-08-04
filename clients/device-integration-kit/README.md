# Device Integration Kit

장치 펌웨어에 통합되는 플랫폼 제공 LwM2M 클라이언트 구성요소다.
완성된 STM32 펌웨어 자체를 제공하는 패키지는 아니다.

## 플랫폼 제공 범위

- 지원 LwM2M Client Engine
- 표준 Object Adapter (`/5` 포함)
- Firmware Update Service
- Device Update Backend 인터페이스
- 상태와 Update Result 변환 로직
- 단위 및 적합성 테스트

## 장치 개발자 구현 범위

- RTOS, 네트워크, DTLS 연결
- 인증정보 저장
- 장치별 Object 데이터
- Flash, 이미지 검증, Bootloader, 재부팅, Confirm, Rollback
- 실제 하드웨어 통합 및 검증

## Linux Reference와의 관계

`clients/linux-reference/`는 이 Kit를 사용하는 POSIX 기반 예제이자 E2E 검증 앱이다.

Linux 소켓, 시그널 처리, 임시 파일, 재부팅 모사는 Linux Reference에만 둔다.
장치에서 재사용할 인터페이스와 로직은 이 Kit에 둔다.

## 메모
//Kit는 “무슨 작업이 필요한가”를 정의하고 장치 개발자는 “그 작업을 어떻게 수행하는가”를 연결합니다
//prepare~recover_after_boot 차례대로 호출
//firmware_backend_status_t: Flash·Bootloader 구현이 반환하는 내부 결과
//firmware_update_result_t: 서버가 /5/0/5로 읽는 LwM2M 표준값

Backend Status: Flash 같은 장치 작업 결과
Update Result: /5/0/5에 보존되는 표준 결과
Service Status: Adapter가 이번 호출의 성공 여부와 CoAP 응답을 판단하는 즉시 결과

//이 매핑 함수가 중요한 이유는 STM32 Flash 구현이 LwM2M 숫자 2, 5, 8 등을 알 필요가 없게 하기 위해서입니다. Backend는 장치 오류만 반환하고 Service가 표준 의미로 번역합니다.