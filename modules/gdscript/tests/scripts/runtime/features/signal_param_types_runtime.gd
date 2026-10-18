extends Node

signal hit(dmg: int, pos: Vector2)
signal legacy(value)

var received := []


func test() -> void:
	var _c1 = hit.connect(func(dmg: int, pos: Vector2): received = [dmg, pos])
	var _c2 = legacy.connect(func(value): received = [value])
	hit.emit(3, Vector2.ONE)
	print(received)
	legacy.emit("old")
	print(received)
	var info: Array = get_signal_list().filter(func(signal_info): return signal_info.name == "hit")
	print(info[0].args)
