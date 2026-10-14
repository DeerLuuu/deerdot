# A trait may itself use traits. Its own `uses` are resolved before its
# members are copied on to the class that uses it.

trait Base:
	var base_value: int = 1

	func base_method() -> String:
		return "base"


trait Derived:
	uses Base
	var derived_value: int = 2


class Consumer extends Node:
	uses Derived


func test():
	var consumer := Consumer.new()
	assert(consumer.base_value == 1)
	assert(consumer.derived_value == 2)
	assert(consumer.base_method() == "base")

	consumer.free()
