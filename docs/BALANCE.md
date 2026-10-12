# Balance

Worked out from the game's tables by `tools/balance/BalanceReport.cpp` (`Economy/Balance.h`); written again after every change to the numbers. Everything is per 100 residents of a tier with every need met and every producer at full productivity; producers are counted in fractions and followed down to their raw goods. Service buildings are counted by the houses one of them serves on average (road range, a house per 15 tiles).

## Summary

| Tier | Taxes / min | Producers' upkeep | Services' upkeep | Net / min | Jobs by tier (F, W, A, E, I) |
|---|---:|---:|---:|---:|---|
| Farmers | 100 | 48 | 3 | **+50** | 21, 0, 0, 0, 0 |
| Workers | 250 | 161 | 4 | **+85** | 41, 36, 0, 0, 0 |
| Artisans | 450 | 207 | 5 | **+238** | 30, 52, 19, 0, 0 |
| Engineers | 700 | 256 | 5 | **+439** | 14, 57, 19, 24, 0 |
| Investors | 1000 | 281 | 4 | **+714** | 4, 58, 0, 31, 15 |

## Farmers (10 per house, 10 houses)

| Need | Per minute |
|---|---:|
| Marketplace | 0.19 buildings |
| Fish | 2.5 |
| Work Clothes | 2.0 |

| Producer | Count | Jobs | Upkeep / min |
|---|---:|---:|---:|
| Fishery | 1.25 | 6.2 Farmers | 12 |
| Sheep Farm | 1.00 | 5.0 Farmers | 10 |
| Framework Knitter | 1.00 | 10.0 Farmers | 25 |

## Workers (20 per house, 5 houses)

| Need | Per minute |
|---|---:|
| Marketplace | 0.09 buildings |
| Fish | 2.5 |
| Work Clothes | 2.0 |
| Sausages | 1.5 |
| Bread | 2.0 |
| Soap | 1.2 |
| School | 0.15 buildings |

| Producer | Count | Jobs | Upkeep / min |
|---|---:|---:|---:|
| Fishery | 1.25 | 6.2 Farmers | 12 |
| Sheep Farm | 1.00 | 5.0 Farmers | 10 |
| Framework Knitter | 1.00 | 10.0 Farmers | 25 |
| Pig Farm | 1.80 | 9.0 Workers | 22 |
| Slaughterhouse | 0.75 | 7.5 Workers | 22 |
| Grain Farm | 1.00 | 10.0 Farmers | 10 |
| Flour Mill | 1.00 | 10.0 Farmers | 15 |
| Bakery | 1.00 | 10.0 Workers | 20 |
| Rendering Works | 0.60 | 3.0 Workers | 9 |
| Soap Factory | 0.60 | 6.0 Workers | 15 |

## Artisans (30 per house, 3 houses)

| Need | Per minute |
|---|---:|
| Marketplace | 0.06 buildings |
| School | 0.10 buildings |
| Sausages | 1.5 |
| Bread | 2.0 |
| Soap | 1.2 |
| Canned Food | 1.2 |
| Sewing Machines | 0.7 |
| Variety Theatre | 0.04 buildings |

| Producer | Count | Jobs | Upkeep / min |
|---|---:|---:|---:|
| Lumberjack | 0.47 | 2.3 Farmers | 2 |
| Pig Farm | 1.80 | 9.0 Workers | 22 |
| Slaughterhouse | 0.75 | 7.5 Workers | 22 |
| Grain Farm | 1.00 | 10.0 Farmers | 10 |
| Flour Mill | 1.00 | 10.0 Farmers | 15 |
| Bakery | 1.00 | 10.0 Workers | 20 |
| Rendering Works | 0.60 | 3.0 Workers | 9 |
| Soap Factory | 0.60 | 6.0 Workers | 15 |
| Cattle Farm | 1.20 | 6.0 Farmers | 12 |
| Iron Mine | 0.95 | 9.5 Workers | 24 |
| Charcoal Kiln | 0.35 | 1.8 Farmers | 4 |
| Furnace | 0.35 | 7.0 Workers | 10 |
| Cannery | 0.60 | 12.0 Artisans | 24 |
| Sewing Machine Factory | 0.35 | 7.0 Artisans | 18 |

## Engineers (40 per house, 2 houses)

| Need | Per minute |
|---|---:|
| Marketplace | 0.05 buildings |
| Variety Theatre | 0.03 buildings |
| Canned Food | 1.2 |
| Sewing Machines | 0.7 |
| Glasses | 0.7 |
| Light Bulbs | 1.0 |
| University | 0.03 buildings |

| Producer | Count | Jobs | Upkeep / min |
|---|---:|---:|---:|
| Lumberjack | 0.80 | 4.0 Farmers | 4 |
| Cattle Farm | 1.20 | 6.0 Farmers | 12 |
| Iron Mine | 0.95 | 9.5 Workers | 24 |
| Charcoal Kiln | 0.85 | 4.2 Farmers | 8 |
| Furnace | 0.35 | 7.0 Workers | 10 |
| Cannery | 0.60 | 12.0 Artisans | 24 |
| Sewing Machine Factory | 0.35 | 7.0 Artisans | 18 |
| Sand Mine | 0.85 | 8.5 Workers | 17 |
| Glassworks | 0.85 | 17.0 Workers | 34 |
| Copper Mine | 0.35 | 5.2 Workers | 10 |
| Spectacle Factory | 0.35 | 8.8 Engineers | 24 |
| Filament Factory | 0.50 | 10.0 Workers | 30 |
| Light Bulb Factory | 0.50 | 15.0 Engineers | 40 |

## Investors (50 per house, 2 houses)

| Need | Per minute |
|---|---:|
| Marketplace | 0.04 buildings |
| University | 0.02 buildings |
| Glasses | 0.7 |
| Light Bulbs | 1.0 |
| Champagne | 0.7 |
| Jewelry | 0.5 |
| Bank | 0.02 buildings |

| Producer | Count | Jobs | Upkeep / min |
|---|---:|---:|---:|
| Lumberjack | 0.33 | 1.7 Farmers | 2 |
| Charcoal Kiln | 0.50 | 2.5 Farmers | 5 |
| Sand Mine | 1.20 | 12.0 Workers | 24 |
| Glassworks | 1.20 | 24.0 Workers | 48 |
| Copper Mine | 0.35 | 5.2 Workers | 10 |
| Spectacle Factory | 0.35 | 8.8 Engineers | 24 |
| Filament Factory | 0.50 | 10.0 Workers | 30 |
| Light Bulb Factory | 0.50 | 15.0 Engineers | 40 |
| Vineyard | 0.70 | 7.0 Workers | 18 |
| Champagne Cellar | 0.35 | 8.8 Investors | 31 |
| Gold Mine | 0.38 | 7.5 Engineers | 19 |
| Goldsmiths | 0.25 | 6.2 Investors | 30 |

## Producers

A good's chain upkeep is what its producer and every producer feeding it cost per good made. A trader pays the good's value (and asks half as much again), so making goods to sell only pays where the value is higher.

| Producer | Makes / min | Workers | Upkeep / min | Chain upkeep per good | Trader pays |
|---|---:|---:|---:|---:|---:|
| Fishery | 2.0 Fish | 5 Farmers | 10 | 5.0 | 6 |
| Lumberjack | 3.0 Wood | 5 Farmers | 5 | 1.7 | 2 |
| Sawmill | 3.0 Planks | 5 Farmers | 10 | 5.0 | 6 |
| Sheep Farm | 2.0 Wool | 5 Farmers | 10 | 5.0 | 6 |
| Framework Knitter | 2.0 Work Clothes | 10 Farmers | 25 | 17.5 | 19 |
| Pig Farm | 1.5 Pigs | 5 Workers | 12 | 8.0 | 9 |
| Slaughterhouse | 2.0 Sausages | 10 Workers | 30 | 23.0 | 25 |
| Grain Farm | 2.0 Grain | 10 Farmers | 10 | 5.0 | 6 |
| Flour Mill | 2.0 Flour | 10 Farmers | 15 | 12.5 | 14 |
| Bakery | 2.0 Bread | 10 Workers | 20 | 22.5 | 25 |
| Rendering Works | 2.0 Tallow | 5 Workers | 15 | 15.5 | 17 |
| Soap Factory | 2.0 Soap | 10 Workers | 25 | 28.0 | 31 |
| Clay Pit | 2.0 Clay | 10 Workers | 15 | 7.5 | 8 |
| Brick Factory | 2.0 Bricks | 15 Workers | 20 | 17.5 | 19 |
| Cattle Farm | 1.0 Beef | 5 Farmers | 10 | 10.0 | 11 |
| Iron Mine | 2.0 Iron | 10 Workers | 25 | 12.5 | 14 |
| Charcoal Kiln | 2.0 Coal | 5 Farmers | 10 | 6.7 | 7 |
| Furnace | 2.0 Steel | 20 Workers | 30 | 34.2 | 38 |
| Steelworks | 1.3 Steel Beams | 20 Workers | 40 | 64.2 | 71 |
| Cannery | 2.0 Canned Food | 20 Artisans | 40 | 42.5 | 47 |
| Sewing Machine Factory | 2.0 Sewing Machines | 20 Artisans | 50 | 60.8 | 67 |
| Sand Mine | 2.0 Quartz Sand | 10 Workers | 20 | 10.0 | 11 |
| Glassworks | 2.0 Glass | 20 Workers | 40 | 30.0 | 33 |
| Window Makers | 1.3 Windows | 20 Artisans | 50 | 69.2 | 76 |
| Copper Mine | 2.0 Copper | 15 Workers | 30 | 15.0 | 17 |
| Spectacle Factory | 2.0 Glasses | 25 Engineers | 70 | 80.0 | 88 |
| Filament Factory | 2.0 Filaments | 20 Workers | 60 | 36.7 | 40 |
| Light Bulb Factory | 2.0 Light Bulbs | 30 Engineers | 80 | 106.7 | 117 |
| Vineyard | 1.0 Grapes | 10 Workers | 25 | 25.0 | 28 |
| Champagne Cellar | 2.0 Champagne | 25 Investors | 90 | 100.0 | 110 |
| Gold Mine | 1.3 Gold | 20 Engineers | 50 | 37.5 | 41 |
| Goldsmiths | 2.0 Jewelry | 25 Investors | 120 | 97.5 | 107 |
