---
status: Active
type: change
specs:
  - docs/specs/project-rules.md
  - docs/specs/project-structure.md
  - docs/guides/working-rules.md
---

# Asset Locking

## 목표

Binary asset 동시 수정 충돌 예방. Git LFS object 저장 X, LFS locking만 사용.

## 전제

| 영역           | 담당                          |
| -------------- | ----------------------------- |
| Unreal Editor  | `Check Out`만 수행            |
| CLI/Fork       | commit, push, PR, merge       |
| Git hooks      | 로컬 실수 방지, LFS lock 검증 |
| GitHub Actions | main 반영 후 자동 unlock      |
| Manual script  | dangling lock 수동 정리       |

팀 모델: 4명 규모, 동등 작업자, 악의 방어 X, 실수 방지 우선.

## 정책

| 항목                       | 결정                                      |
| -------------------------- | ----------------------------------------- |
| Asset 저장                 | Git blob 유지                             |
| LFS 사용                   | lock, read-only, push verification만      |
| `.gitattributes`           | `filter=lfs` X, `lockable` O              |
| `git lfs track --lockable` | 사용 X                                    |
| UE Git provider            | ProjectBorealis UEGitPlugin 적용          |
| Editor                     | checkout only, submit/push/unlock X       |
| Fork                       | hook 실행 가정, skip hooks는 의도적 우회  |
| main local commit          | 차단                                      |
| main local merge           | 차단                                      |
| main local delete          | 허용                                      |
| main 일반 push             | 차단                                      |
| main force push            | 허용                                      |
| 최초 main 생성             | 허용                                      |
| main force push 후 lock    | force push 이전 Unreal asset lock cleanup |
| manual unlock              | 사람이 exact path 지정                    |
| AI agent                   | manual unlock script 자동 호출 금지       |

## 설정

### Attributes

```gitattributes
[attr]ue-lock -diff -merge -text -eol lockable

*.uasset ue-lock
*.umap ue-lock
*.ubulk ue-lock
*.uexp ue-lock
```

대상:

- `.gitattributes`
- 필요 시 `Client/.gitattributes`
- `.lfsconfig`: `lfs.locksverify=true`

### Git config

Git 설정 진입점은 `tools/set-git-config.ps1`.

```powershell
git config --local core.hooksPath .githooks
git lfs install --local --manual
git lfs update --manual
git config --local merge.ff false
git config --local lfs.locksverify true
git config --local lfs.setlockablereadonly true
git lfs post-checkout 0000000000000000000000000000000000000000 HEAD 1
```

이미 적용된 Git config 값과 성공 검증은 출력하지 않고, 변경이 필요한 값과 완료 문구만 로그를 남긴다.
script 실행 시 `.lfsconfig`와 Unreal asset attribute도 함께 확인한다.

`task-setup.bat`: 새 script 호출.

초기 clone checkout은 hook 설치 전이므로 read-only가 아닐 수 있다. `task-setup.bat` 완료 시 current checkout에 read-only 상태를 재적용한다.

### Hooks

`.githooks/reference-transaction`:

- `refs/heads/main` local delete → 허용
- `refs/heads/main` fast-forward update → remote main sync만 허용
- `refs/heads/main` non-fast-forward update → 허용
- 기존 branch naming 검증 유지

`.githooks/pre-commit`, `.githooks/pre-merge-commit`:

- main direct commit early reject
- main local merge early reject
- feature branch에서는 source code sanity check skip
- PR source code sanity Action에서 staged source diff 검증
- local merge 적용 후 commit 전 차단되므로 사용자는 `git merge --abort`로 정리

`.githooks/pre-push`:

- local policy/lock 검증 후 Git LFS `pre-push` 호출
- `refs/heads/main` delete → 차단
- `refs/heads/main` creation → 허용
- `refs/heads/main` fast-forward push → 차단
- `refs/heads/main` non-fast-forward push → 허용
- main non-fast-forward push는 asset lock 검증 skip
- lockable asset push 시 active lock 검증

구현 기준: `--force` flag가 아니라 remote ref 기준 non-fast-forward 여부.
remote에 `main`이 없으면 creation으로 보이므로 허용한다.

`.githooks/post-checkout`, `.githooks/post-commit`, `.githooks/post-merge`:

- Git LFS lockable read-only 상태 재적용
- `post-commit`은 HEAD commit에 Unreal binary asset 변경이 없으면 skip

### Unreal Editor

- `Check Out` 시 LFS lock 확인
- 기존 lock 존재 → 실패
- lock 성공 → writable
- Submit, Push, Unlock 사용 X

Provider:

- ProjectBorealis UEGitPlugin 적용 확정
- `lockable` only 인식 검증
- provider 동작은 `git check-attr lockable` 기준
- plugin initialize UI는 `filter=lfs`를 만들 수 있으므로 사용 금지
- provider가 실제 동작 중 `filter=lfs`를 요구하면 작업 중단, LFS object 전환 X
- `git check-attr lockable -- "*.uasset" "*.umap"` 통과
- `OdiroSimEditor Win64 Development` build 통과

## GitHub Actions

### Pull request checks

파일: `.github/workflows/pull-request-check.yml`

트리거: `pull_request`

`Source Sanity`:

- PR merge ref checkout
- merge ref의 first parent 기준 `git reset --soft`
- PR 변경분을 staged diff처럼 구성
- `tools/check-source-sanity.ps1 -Hook pr-check -Force`

`Asset Lock Ownership`:

- PR 변경 Unreal asset 목록
- active LFS lock 존재
- lock owner = PR author
- branch 분기 후 main의 동일 asset 변경 없음

실패:

- lock 없음
- 타인 lock
- stale asset

### Post-merge asset unlock

파일: `.github/workflows/post-merge-task.yml`

트리거:

- `push` to `main`
- `workflow_dispatch`

일반 main update:

- `before..after` 변경 Unreal asset 계산
- active lock 조회
- lock 존재 시 `git lfs unlock --force`
- 이미 unlock → no-op

force push:

- `github.event.forced == true`
- `before..after` 변경 Unreal asset만 cleanup 대상으로 제한
- GitHub push 시각 기준 cutoff 이후 생성된 lock 유지
- unlock 직전 lock id와 locked_at 재조회
- before commit fetch 실패 시 fail-closed 후 수동 unlock 사용

실패:

- lock 유지
- manual unlock script로 정리

수동 GitHub 작업:

- 첫 commit staging 시 `.gitmodules`와 `Client/Plugins/UEGitPlugin` 포함
- `Client/Plugins/UEGitPlugin`은 vendored files가 아니라 mode `160000` gitlink인지 확인
- Actions enabled 확인
- `LFS_LOCK_BOT_TOKEN` secret 등록
- lock owner가 GitHub login과 다르면 `LFS_LOCK_OWNER_ALIASES` variable 등록
- workflow 1회 실행 후 check 이름 확인
- PR check required 여부 결정
- required check와 main force push 정책 충돌 확인

## Manual unlock

파일: `tools/manual-unlock.ps1`

규칙:

- exact path 필수
- wildcard, 빈 path, repository root 거부
- 기본 dry-run
- 실제 unlock: `-Unlock` + 확인 문구 필수
- 출력: lock id, path, owner, locked_at
- Coding AI 자동 호출 금지

Agent 지침:

```text
Coding agents must never run manual LFS unlock scripts unless the user explicitly requests unlock for exact paths in the current turn.
```

## 변경 파일

```text
.gitmodules
.gitattributes
.lfsconfig
.githooks/post-checkout
.githooks/post-commit
.githooks/post-merge
.githooks/pre-commit
.githooks/pre-merge-commit
.githooks/pre-push
.githooks/reference-transaction
Client/Plugins/UEGitPlugin
Client/OdiroSim.uproject
tools/set-git-config.ps1
tools/install.ps1
tools/check-prerequisites.ps1
tools/pre-push-policy.ps1
tools/manual-unlock.ps1
docs/guides/working-rules.md
docs/guides/development-environment.md
docs/specs/project-structure.md
AGENTS.md
.agents/index/root-dev-workflow.md
.agents/index/client-runtime-foundation.md
.github/workflows/pull-request-check.yml
.github/workflows/post-merge-task.yml
```

## 작업

### T01 Attributes와 LFS config [x]

- `.gitattributes`: `ue-lock` macro 추가
- Unreal binary 확장자에 `ue-lock` 적용
- `Client/.gitattributes` 중복 정책 정리
- `.lfsconfig`: `lfs.locksverify=true`

**검증**

```powershell
git check-attr -a -- Client/Content/<sample>.uasset
git check-attr lockable -- Client/Content/<sample>.uasset
```

### T02 Git config script [x]

- `tools/set-git-config.ps1` 추가
- 기존 `core.hooksPath`, `merge.ff=false` 유지
- `pull.ff=only` 설정으로 `git pull`은 fast-forward sync만 허용
- LFS manual install/update/config 추가
- 이미 설정된 Git config 값과 성공 검증은 생략하고 변경된 값과 완료 문구만 표시
- `.lfsconfig`, Unreal asset attribute 확인
- current checkout read-only 재적용
- `task-setup.bat`에서 Git submodule update 수행
- `tools/install.ps1`, docs 참조 갱신
- 기존 hook 전용 script 삭제

**검증**

```powershell
.\tools\set-git-config.ps1
git config --local --get core.hooksPath
git config --local --get merge.ff
git config --local --get pull.ff
git config --local --get lfs.locksverify
git config --local --get lfs.setlockablereadonly
```

### T03 main local update hook [x]

- `reference-transaction` main ref update 검증
- `pre-commit`, `pre-merge-commit` main early reject
- main local delete 허용
- main fast-forward update는 remote main sync만 허용
- main non-fast-forward update 허용
- branch naming 검증 유지
- remote main sync 허용, local feature fast-forward 차단 검증

**검증**

```powershell
git checkout main
git commit --allow-empty -m "test: should fail"
git merge --no-ff <test-branch>
git merge --ff-only <test-branch>
```

검증은 throwaway branch/commit에서 수행.

### T04 pre-push hook [x]

- `.githooks/pre-push` 추가
- Git LFS `pre-push` 호출
- main delete 차단
- main creation 허용
- main fast-forward push 차단
- main non-fast-forward push 허용
- main non-fast-forward push에서 asset lock 검증 skip
- Unreal binary asset push 시 active lock 검증
- `tools/pre-push-policy.ps1` 사용
- new branch push는 remote main 기준 변경 범위 확인, 없으면 local main fallback

**검증**

```powershell
git push origin main
git push --force-with-lease origin main
git lfs locks --verify
```

검증은 test remote 또는 throwaway repo에서 수행.

복구 또는 최초 main 생성:

```powershell
git push --force origin HEAD:main
```

### T05 pull request check Action [x]

- `.github/workflows/pull-request-check.yml`
- Source Sanity job
- PR changed files 계산
- LFS lock 목록 조회
- PR author와 lock owner 매핑
- optional `LFS_LOCK_OWNER_ALIASES` variable 지원
- stale asset 검증
- 실패 message: path, lock owner, 조치
- local fake PR ref no-op runtime 통과

**검증**

```text
lock 없음 -> 실패
본인 lock -> 성공
타인 lock -> 실패
main 동일 asset 선변경 -> 실패
```

### T06 main auto unlock Action [x]

- `.github/workflows/post-merge-task.yml`
- 일반 update와 force update 분기
- 일반 update: 변경 asset만 unlock
- force update: 변경 asset만 unlock 대상으로 제한하고 lock id/time 재확인
- `workflow_dispatch`: dry-run 또는 exact path unlock
- secret: `LFS_LOCK_BOT_TOKEN`
- local dry-run/no-op/force no-op runtime 통과

**검증**

```text
PR merge 후 변경 asset unlock
이미 unlock된 asset no-op
bot token 권한 없음 -> 실패, lock 유지
main force push 후 기존 lock cleanup
force push 이후 신규 lock 유지
```

### T07 manual unlock script [x]

- `tools/manual-unlock.ps1`
- dry-run 기본값
- unlock은 exact path + confirm 요구
- wildcard/빈 path/root 거부
- help에 AI 자동 호출 금지 명시

**검증**

```powershell
.\tools/manual-unlock.ps1 -Path Client/Content/<sample>.uasset
.\tools/manual-unlock.ps1 -Path Client/Content/<sample>.uasset -Unlock -ConfirmPath Client/Content/<sample>.uasset
```

### T08 문서 갱신 [x]

- `docs/guides/development-environment.md`: Git LFS setup
- `docs/guides/working-rules.md`: checkout, PR, main force push, manual unlock
- `docs/specs/project-structure.md`: `.githooks`, `.github/workflows`, tools
- `AGENTS.md`: manual unlock 자동 호출 금지
- `.agents/index`: path/verification flow 갱신

**검증**

```powershell
rg "set-git-config|manual-unlock|LFS|lock" docs AGENTS.md task-setup.bat
.\tools\set-git-config.ps1
```

### T09 End-to-end [ ]

- clean clone에서 `task-setup.bat`
- setup 완료 후 sample asset read-only 확인
- Editor checkout → lock + writable 확인
- 다른 clone checkout 실패 확인
- read-only 수동 해제 후 PR check 실패 확인
- PR merge 후 auto unlock 확인
- dangling lock manual unlock 확인
- main force push 후 기존 lock cleanup 확인
- Fork push hook 실행 확인

**검증**

```powershell
git lfs locks
git lfs locks --verify
git status --short --branch
git check-attr -a -- Client/Content/<sample>.uasset
```

## 위험

| 위험                          | 대응                                  |
| ----------------------------- | ------------------------------------- |
| read-only 수동 해제           | PR lock check, pre-push lock 검증     |
| hook skip                     | 의도적 우회, 팀 규칙 처리             |
| Fork hook 미실행              | 검증 후 CLI push 제한                 |
| auto unlock 실패              | lock 유지, manual unlock              |
| bot token 권한 과다           | repo scope 최소화                     |
| main branch가 remote에 없음   | main creation push 허용               |
| main force push 후 stale lock | force event 이전 lock cleanup         |
| force cleanup race            | `locked_at`이 force event 이후면 유지 |
| force cleanup cutoff 보수성   | stale lock 유지, manual unlock        |
| plugin이 `filter=lfs` 요구    | 작업 중단, LFS object 전환 X          |

## 완료 기준

- setup 한 번으로 hook과 LFS lock config 적용
- Unreal binary asset = Git blob
- setup 완료 후 lock 전 asset read-only
- Editor checkout = lock + writable
- main 일반 commit/local merge/fast-forward push 차단
- main local delete 허용
- main non-fast-forward push 허용
- PR lock check 동작
- main 반영 후 auto unlock 동작
- dangling lock = manual script로만 정리
- AI manual unlock 자동 호출 금지 문서화

## References

- [Git LFS File Locking API](https://github.com/git-lfs/git-lfs/blob/main/docs/api/locking.md)
- [git-lfs-lock](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-lock.adoc)
- [git-lfs-config](https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-config.adoc)
- [Git githooks](https://git-scm.com/docs/githooks)
- [GitHub push webhook payload](https://docs.github.com/en/webhooks/webhook-events-and-payloads#push)
- [GitHub Actions workflow syntax](https://docs.github.com/actions/using-workflows/workflow-syntax-for-github-actions)
- [GitHub manual workflows](https://docs.github.com/actions/managing-workflow-runs/manually-running-a-workflow)
