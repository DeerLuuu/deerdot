extends Node

signal hit(dmg: int, pos: Vector2)

var received := []


func test() -> void:
	var _c = hit.connect(func(dmg: int, pos: Vector2 = Vector2.ZERO): received = [dmg, pos])
	var v: Variant = Vector2.ONE
	hit.emit(3, v)
	print(received)
