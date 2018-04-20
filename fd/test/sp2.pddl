(define (problem carry-test)
(:domain carry-strips)

(:objects 
l1 l2 l3 - city
t0 - truck
p1 p2 - package
)

(:init
(next l1 l2)(next l2 l3)
(next l2 l1)(next l3 l2)

(truck-at t0 l2)
(package-at p1 l1)
(package-at p2 l3)


(= (distance l1 l2) 2) (= (distance l2 l3) 1) 
(= (distance l2 l1) 2) (= (distance l3 l2) 1)
(= (total-cost) 0)

)

(:goal
(and
(package-at p1 l3)
(package-at p2 l1)
))

(:metric minimize (total-cost))

)
