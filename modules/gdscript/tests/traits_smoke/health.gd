## 生命值系统：可复用的血量与治疗。
## 用文件级声明（`trait Health`，无冒号无缩进）。
trait Health

var health := 100
var max_health := 100

func heal(amount: int) -> void:
	health = mini(health + amount, max_health)

func is_alive() -> bool:
	return health > 0
