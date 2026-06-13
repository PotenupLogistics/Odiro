# 작업 공통 규칙

- 생성물, cache, build output 업로드 금지
- 실행 경로 임의 추측 금지
- 문서, 구현, 테스트 일관성 유지

## 보안 규칙

API key, secret, credential, token 등 민감한 정보는 절대 커밋하지 않는다.

```text
.env
.env.local
secrets/
*.pem
*.key
OAuth token
service account private key
LLM API key
```

외부 접속이 가능한 API는 기본적으로 다음을 요구한다.

- 명시적 enable 옵션
- 인증 또는 access token
- CORS 제한
- User Directory path validation
- 내부 경로와 stack trace 비노출

## 구현 규칙

- 기존 기능과 호환되지 않는 변경은 `feat` 또는 `fix`로 명확히 구분하여 커밋한다.
- API 변경 시 `contracts` 함께 갱신
- 타인 코드 수정 최소화
- 객체지향 설계 원칙 준수하여 모듈 간 의존성 최소화

## Main merge 규칙

- `main`은 fast-forward, direct update를 허용하지 않고 merge commit으로만 갱신한다.
- 소스코드 변경 시 `main` merge 직전 빌드를 통과해야 한다.
