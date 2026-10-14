# Resolution runs after inheritance, so a trait member can override one
# that came from the base class.

trait Counter:
	var value: int = 0

	func describe() -> String:
		return "trait"


class Ancestor extends Node:
	func describe() -> String:
		return "ancestor"


class Descendant extends Ancestor:
	uses Counter


func test():
	var instance := Descendant.new()
	# The trait's method wins over the inherited one.
	assert(instance.describe() == "trait")
	# The trait's variable is present as well.
	assert(instance.value == 0)

	instance.free()
