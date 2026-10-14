# A class that uses a trait passes the trait's members down to its own
# subclasses, the same way inherited members would be reached.

trait Greets:
	var greeting: String = "hello"

	func greet() -> String:
		return greeting


class Base extends Node:
	uses Greets


class Leaf extends Base:
	pass


func test():
	var leaf := Leaf.new()
	assert(leaf.greeting == "hello")
	assert(leaf.greet() == "hello")

	leaf.greeting = "hi"
	assert(leaf.greet() == "hi")

	leaf.free()
