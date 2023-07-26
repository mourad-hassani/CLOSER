{ zone2sensor(Z,S) : zone(Z), sensor(S) }.
#instance zone2sensor/2.

1 { unit2zone(U,Z) : unit(U) } 1 :- zone(Z).
1 { unit2sensor(U,S) : unit(U) } 1 :- sensor(S).

:- unit(U), unitCap(C), C + 1 { unit2zone(U,Z) : zone(Z) }.
:- unit(U), unitCap(C), C + 1 { unit2sensor(U,S) : sensor(S) }.

partnerunits(U,P) :- unit2zone(U,Z), zone2sensor(Z,S), unit2sensor(P,S), U != P.
partnerunits(U,P) :- partnerunits(P,U), unit(U), unit(P).

:- unit(U), interUnitCap(C), C + 1 { partnerunits(U,P) : unit(P) }.

zone(1..20).
sensor(1..28).
unit(1..16).
unitCap(2).
interUnitCap(2).