# is on a trait asks whether a class took it, which is known while the
# script is analyzed.

trait T:
	var x := 1


class Uses:
	uses T


class Plain:
	var x := 1


class Child extends Uses:
	pass


func test():
	var user := Uses.new()
	print(user is T)

	var plain := Plain.new()
	print(plain is T)

	# A class that extends one which took the trait has it as well.
	var child := Child.new()
	print(child is T)
