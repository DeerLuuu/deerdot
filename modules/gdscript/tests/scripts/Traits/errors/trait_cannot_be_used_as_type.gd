# A trait is not a type: it cannot be used as one.

trait T:
	var x := 1


class Holder:
	trait Inner:
		var y := 2


class Uses extends Node:
	uses T


var t: T = null

var inner: Holder.Inner = null

var inners: Array[Holder.Inner] = []


func f() -> T:
	return null


func g() -> Holder.Inner:
	return null


func test():
	var casted = null as Holder.Inner

	# A class object is not an instance that took the trait, just like
	# `Uses is Uses` is rejected: a type test asks about a value.
	print(Uses is T)
