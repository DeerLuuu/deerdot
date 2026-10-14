# Members of a used trait are copied into the class, so each instance keeps
# its own state instead of sharing it the way a base class would.

trait Damageable:
	var health: int = 100

	func take_damage(amount: int) -> void:
		health -= amount

	func is_dead() -> bool:
		return health <= 0


class Actor extends Node:
	uses Damageable

	func take_damage(amount: int) -> void:
		health -= amount * 2


func test():
	var first := Actor.new()
	var second := Actor.new()

	# The class's own method wins over the trait's, so 10 becomes 20.
	first.take_damage(10)
	assert(first.health == 80)
	assert(not first.is_dead())

	# The second instance is unaffected: the trait's members were copied
	# into Actor, not shared through it.
	assert(second.health == 100)
	assert(second.is_dead() == false)

	first.free()
	second.free()
