(define (problem carry-test2)
(:domain carry-strips)

(:objects 
l1 l2 - city
t0 - truck
p1 p2 - package
)

(:init
(next l1 l2)(next l2 l1)

(truck-at t0 l1)
(package-at p1 l2)
(package-at p2 l1)


(= (distance l1 l2) 1)
(= (distance l2 l1) 1)
(= (total-cost) 0)

)

(:goal
(and
(package-at p1 l1)
(package-at p2 l2)
))

(:metric minimize (total-cost))

)