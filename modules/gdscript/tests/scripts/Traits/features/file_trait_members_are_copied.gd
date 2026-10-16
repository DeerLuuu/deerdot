# A trait declared as 	rait Name at the top of a file describes the whole
# file, so the file's top level members are its members.

trait Damageable

var health: int = 100

func take_damage(amount: int) -> void:
	health -= amount

func is_dead() -> bool:
	return health <= 0


class Actor extends Node:
	uses Damageable


func test():
	var actor := Actor.new()
	actor.take_damage(10)
	assert(actor.health == 90)
	assert(not actor.is_dead())
	actor.free()
