# A project trait that describes a whole file.

trait Heals

var hp := 100

func heal(amount: int) -> void:
	hp += amount
