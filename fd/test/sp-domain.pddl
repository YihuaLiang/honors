(define (domain travel-strips)
	(:requirements	:typing :action-costs)
	(:types		truck - object
			city - object
	)

(:predicates
	(truck-at ?t - truck ?l - city)
	(next ?l1 - city ?l2 - city)
	(visited ?l - city)
)

(:functions
	(total-cost) - number
        (distance ?l1 city ?l2 - city) - number
)

(:action drive
  :parameters (?t - truck ?l1 - city ?l2 - city )
  :precondition (and (truck-at ?city ?l1) (next ?l1 ?l2 ))
  :effect (and (truck-at ?t ?l2) (visited ?l2) (not (truck-at ?t ?l1)) (increase (total-cost) (distance ?l1 ?l2))))
)
