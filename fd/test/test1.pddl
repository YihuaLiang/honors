(define (problem transport-l9-t2-p9---int100n150-m25---int100c100---s3703712---e0)
(:domain transport-strips)

(:objects
l0 l1 l2 l3 l4 l5 l6 l7 l8 - location
t0 t1 - truck
p0 p1 p2 p3 p4 p5 p6 p7 p8 - package
)

(:init
(connected l0 l1)
(fuelcost level7 l0 l1)
(connected l0 l4)
(fuelcost level4 l0 l4)
(connected l0 l7)
(fuelcost level16 l0 l7)
(connected l1 l0)
(fuelcost level7 l1 l0)
(connected l1 l6)
(fuelcost level21 l1 l6)
(connected l2 l6)
(fuelcost level5 l2 l6)
(connected l2 l7)
(fuelcost level24 l2 l7)
(connected l3 l5)
(fuelcost level10 l3 l5)
(connected l3 l7)
(fuelcost level16 l3 l7)
(connected l3 l8)
(fuelcost level12 l3 l8)
(connected l4 l0)
(fuelcost level4 l4 l0)
(connected l4 l5)
(fuelcost level3 l4 l5)
(connected l4 l6)
(fuelcost level18 l4 l6)
(connected l4 l8)
(fuelcost level21 l4 l8)
(connected l5 l3)
(fuelcost level10 l5 l3)
(connected l5 l4)
(fuelcost level3 l5 l4)
(connected l6 l1)
(fuelcost level21 l6 l1)
(connected l6 l2)
(fuelcost level5 l6 l2)
(connected l6 l4)
(fuelcost level18 l6 l4)
(connected l7 l0)
(fuelcost level16 l7 l0)
(connected l7 l2)
(fuelcost level24 l7 l2)
(connected l7 l3)
(fuelcost level16 l7 l3)
(connected l7 l8)
(fuelcost level22 l7 l8)
(connected l8 l3)
(fuelcost level12 l8 l3)
(connected l8 l4)
(fuelcost level21 l8 l4)
(connected l8 l7)
(fuelcost level22 l8 l7)

(at t0 l4)
(fuel t0 level106)
(at t1 l3)
(fuel t1 level0)

(at p0 l0)
(at p1 l0)
(at p2 l0)
(at p3 l1)
(at p4 l4)
(at p5 l4)
(at p6 l7)
(at p7 l7)
(at p8 l0)
)

(:goal
(and
(at p0 l5)
(at p1 l5)
(at p2 l1)
(at p3 l0)
(at p4 l0)
(at p5 l2)
(at p6 l1)
(at p7 l8)
(at p8 l7)
)
)
)