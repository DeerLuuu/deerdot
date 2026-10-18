extends Node

signal hit(dmg: int, pos: Vector2)


func test() -> void:
	hit.connect(func(dmg: int): print(dmg))
