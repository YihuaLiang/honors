(define (domain carry-strips)
	(:requirements	:typing :action-costs)
	(:types		truck - object
			city - object
			package - object
	)

(:predicates
	(truck-at ?t - truck ?l - city)
	(next ?l1 - city ?l2 - city)
	(package-at ?p - package ?l - city)
	(package-on ?p - package ?t - truck)
)

(:functions
	(total-cost) - number
        (distance ?l1 - city ?l2 - city) - number
)

(:action drive
  :parameters (?t - truck ?l1 - city ?l2 - city )
  :precondition (and (truck-at ?t ?l1) (next ?l1 ?l2 ))
  :effect (and (truck-at ?t ?l2)(not (truck-at ?t ?l1)) (increase (total-cost) (distance ?l1 ?l2))))

(:action load
	:parameters (?t - truck ?l - city ?p - package)
	:precondition (and(truck-at ?t ?l)(package-at ?p ?l))
	:effect (and (package-on ?p ?t)(not (package-at ?p ?l))))

(:action unload
        :parameters (?t - truck ?l - city ?p - package)
	:precondition (and(truck-at ?t ?l)(package-on ?p ?t))
	:effect (and (package-at ?p ?l)(not (package-on ?p ?t))))
)