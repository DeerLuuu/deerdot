# A trait is not a type: it cannot be used as one.

trait T:
	var x := 1


var t: T = null


func f() -> T:
	return null


func test():
	pass
