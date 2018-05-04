(define (problem travel-test1)
(:domain travel-strips)

(:objects 
l1 l2 l3 - city
t0 - truck
)

(:init
(next l2 l1)(next l1 l3)
(next l1 l2)(next l3 l1)

(truck-at t0 l1)
(visited l1)
(= (distance l1 l3) 1) (= (distance l2 l1) 1)
(= (distance l3 l1) 1) (= (distance l1 l2) 1)
(= (total-cost) 0)

)

(:goal
(and
(visited l1)
(visited l2)
(visited l3)
))

(:metric minimize (total-cost))

)
