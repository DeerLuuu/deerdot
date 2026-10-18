extends Node

@observable
var hp := 10

func _init() -> void:
	var _hp_connection := hp_changed.connect(func(value): print("changed ", value))
	print("init ", hp)
	hp = 20
	hp = 20
	print("done ", hp)

func test() -> void:
	pass
