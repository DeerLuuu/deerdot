# A trait's variable that a base class already declares is not copied: the
# inherited variable is used instead. Copying it used to give the class a
# second member slot for the same name, which crashed when the class was
# instantiated.

trait T:
	var x := 1

	func describe() -> String:
		return "trait"


class A extends Node:
	var x := 2


class B extends A:
	uses T


func test():
	var b := B.new()
	# The inherited variable wins over the trait's copy.
	assert(b.x == 2)
	# Functions are still brought in by the trait.
	assert(b.describe() == "trait")
	b.free()
