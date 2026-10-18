extends Node

signal hit(dmg: int)


func test() -> void:
	hit.connect(func(dmg: int, extra: int): print(dmg, extra))
