# user_agent.py에 있는 create_policy 함수를 agent 패키지 밖에서 쉽게 import하기 위해 가져온다.
from .user_agent import create_policy


# from agent import * 를 사용할 때 외부에 공개할 이름을 제한한다.
# create_policy만 외부 공개 대상으로 둔다.
__all__ = ["create_policy"]