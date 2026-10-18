extends Node

signal hit(dmg: int)


func test() -> void:
	hit.emit("bad")
	emit_signal("hit", "bad")
