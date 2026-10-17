extends SceneTree

# 语言特性冒烟测试：验证 trait 的复制语义、跨文件使用、块式/文件级两种声明、is 判定。
# 注意：先加载声明 trait 的脚本，让全局注册表知道它们（无编辑器时项目扫描不运行）。

func _initialize() -> void:
	load("res://health.gd")
	load("res://armored.gd")

	var EnemyScript = load("res://enemy.gd")
	var PlainScript = load("res://plain.gd")

	var enemy = EnemyScript.new()
	print("start health: ", enemy.health)
	print("start armor: ", enemy.armor)

	enemy.hurt(25) # 护甲吸收 10，血量 -15
	print("health after hurt(25): ", enemy.health)
	print("armor after hurt(25): ", enemy.armor)

	enemy.heal(30) # 上限 100
	print("health after heal(30): ", enemy.health)

	print("enemy takes Health: ", enemy.takes_health())
	print("enemy takes Armored: ", enemy.takes_armored())

	var plain = PlainScript.new()
	print("plain takes Health: ", plain.takes_health())

	# 每个实例状态独立（成员是复制，不是继承）
	var other = EnemyScript.new()
	print("other health: ", other.health)

	quit()
