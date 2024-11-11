# Pintos Assn2 Final Report
Team 37 
20200229 김경민, 20200423 성치호 

## 구현
### Argument Passing
디자인 레포트에서 제안한 구현 방식에서 벗어나지 않으나 디자인 레포트에서는 세부적인 사항들을 포함하지 않아 이를 바탕으로 구현하는 과정에서 디자인 레포트에서는 언급하지 않은 세부적인 사항이 많이 추가되었다. 아래 사항들이 있다.
- argument string, argument 길이 array들을 저장하기 위해 page allocation을 사용.
- `process_execute`, `start_process`에서만 구현 사항을 추가하는 기존 계획 --> 예상보다 구현의 복잡도와 높아 Argument Passing 기능을 여러 함수에 걸쳐 수행할 수 있게 함. `process_execute`, `start_process`, `parse_args`, `setup_args_stack` 에 걸쳐서 작동.
	- `parse_args`, `setup_args_stack` 함수 추가
- 기존 계획: `process_execute`에서 받은 `file_name`을 parsing하여 앞 부분을 스레드 생성 이름으로, 뒷 부분을 

### System Call 기반
### System Call - PCB

## 발전한 점
## 한계
## 배운 점