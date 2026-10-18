extends Node

signal hit(dmg: int, pos: Vector2)
signal ping()
signal legacy(value)


func test() -> void:
	hit.emit(3, Vector2.ONE)
	ping.emit()
	var dynamic_name := "ping"
	var _dynamic = emit_signal(dynamic_name, "not checked")
	var _emitted = emit_signal("hit", 4, Vector2.ZERO)
	legacy.emit("anything")
	var _c1 = hit.connect(func(dmg: int, pos: Vector2): print(dmg, pos))
	var _c2 = hit.connect(func(dmg: int): print(dmg))
	var _c3 = hit.connect(func(): print("no args"))
	var _c4 = legacy.connect(func(value): print(value))
	print(get_signal_list().filter(func(s): return s.name == "hit"))
