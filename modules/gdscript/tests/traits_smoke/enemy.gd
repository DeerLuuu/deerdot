class_name Enemy

# 跨文件使用两个 trait（一个文件级、一个块式）。
uses Health
uses Armored

## 先被护甲吸收，再扣血。
func hurt(amount: int) -> void:
	health -= absorb(amount)


# `is` 的语义：是否使用了该 trait（包含继承）。
func takes_health() -> bool:
	return self is Health


func takes_armored() -> bool:
	return self is Armored
