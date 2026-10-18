extends Node

signal hit(dmg: int)


func test() -> void:
	hit.connect(func(dmg: String): print(dmg))
