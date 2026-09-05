# Shadowdark Torch — M5Stack CoreS3

Contador de antorcha para **Shadowdark RPG**. En Shadowdark una antorcha dura
**una hora de tiempo real**: este cacharro la lleva por ti en la mesa, sin
cronómetros de móvil ni cuentas mentales.

```
      /\        antorcha en pixel art, la llama se anima a 4 fps
     /  \
    ( ** )
      ||
   [========------------]   barra = 200 fragmentos, 85% del ancho
```

## Cómo se juega

| Acción | Gesto |
|---|---|
| **Encender** | Sacude el CoreS3 con ganas (varios meneos seguidos) |
| **Apagar** | Sopla fuerte sobre los micrófonos del lateral |
| **Se consume** | Sola, a los 60 minutos |
| **Armar el soplido** | Un toque en la pantalla; otro vuelve a protegerla |

La barra **no muestra minutos**, solo cuánta antorcha queda: 200 fragmentos,
uno cada 18 segundos. En el último 10% la llama se encoge y se apaga a
ratos; en el último 5% la barra late. Cuando llega a cero la antorcha se
apaga y hay que volver a sacudir para prender una nueva.

## Detalles de implementación

**Encendido por sacudida.** No basta un pico de aceleración: se exigen
`SHAKE_PEAKS_NEEDED` (3) picos de más de `SHAKE_PEAK_G` (1,6 g sobre el
reposo) separados al menos 70 ms, todos dentro de una ventana de 1,5 s. Un
golpe a la mesa o alguien que coge el aparato produce un único pico y no
enciende nada.

**Apagado soplando.** El umbral es **adaptativo**: se mide el ruido ambiente
en vivo y se exige superarlo `BLOW_FLOOR_RATIO` (6×) durante
`BLOW_SUSTAIN_MS` (640 ms) seguidos. Así funciona igual en una mesa callada
que en un bar, y hay que soplar de verdad: medio segundo largo descarta
palmadas, plosivas y golpes en la mesa, que duran una décima.

**Candado del soplido.** Una antorcha recién encendida **no se puede apagar
soplando**: hace falta tocar la pantalla para armarla. El candado bajo la
barra lo dice sin palabras — cerrado y apagado significa a salvo, abierto y
ámbar significa que el próximo soplido la apaga.

Es la defensa contra el problema de verdad de este cacharro: el micrófono no
distingue un soplido de cualquier otro ruido fuerte y cercano, así que en vez
de afinar umbrales hasta la extenuación, la antorcha simplemente no escucha
hasta que tú se lo pides. Apagarla pasa a ser deliberado: tocar y soplar.

El candado vuelve a ponerse solo cada vez que se enciende una antorcha nueva,
así que no se queda armado de una escena para otra.

**Sin parpadeos.** Cada cuadro se compone entero en un lienzo en PSRAM y se
vuelca de golpe.

## Decisiones que tomé por ti

Están todas en [`src/config.h`](src/config.h) como constantes, cambiar
cualquiera es una línea:

| Decisión | Valor | Alternativa |
|---|---|---|
| Cada antorcha **nace protegida**: hay que tocar para poder soplarla | `BLOW_LOCKED_ON_LIGHT = true` | `false` nace armada y el toque sirve para proteger |
| Soplar **apaga la antorcha del todo**: la siguiente sacudida enciende una nueva de 60 min | `RESUME_AFTER_BLOWOUT = false` | `true` guarda el tiempo restante y lo reanuda al reencender |
| Los rótulos están en inglés (`SHAKE TO LIGHT`, `BURNED OUT`) | — | `drawMessage(...)` en `src/main.cpp` |

La segunda fila importa si en tu mesa apagáis la antorcha para *guardarla*:
con `RESUME_AFTER_BLOWOUT = true` soplar deja de gastar antorcha.

## Compilar y flashear

```bash
pio run -t upload && pio device monitor
```

## Calibrar el soplido

El nivel del micrófono depende de tu unidad y de la sala. Si te cuesta
apagarla o se apaga sola, pon `#define MIC_DEBUG 1` en
[`src/main.cpp`](src/main.cpp), abre el monitor serie y sopla:

```
[mic] rms=412  ruido=380  umbral=2280  soplido=0 ms
[mic] rms=9840 ruido=381  umbral=2286  soplido=176 ms   <- soplando
```

Ajusta `BLOW_ABS_MIN_RMS` (suelo absoluto) y `BLOW_FLOOR_RATIO` en
`src/config.h` a partir de esos números. Lo mismo con `SHAKE_PEAK_G` si la
sacudida te resulta dura o blanda.

## Estructura

```
src/config.h      todos los parámetros ajustables
src/torch_art.h   el pixel art (4 fotogramas de llama + cuerpo + apagada)
src/main.cpp      máquina de estados, sensores y dibujo
```

El arte son cadenas de texto, un carácter por píxel: se edita a mano sin
herramientas. La rejilla es de 16 columnas y cada píxel se dibuja a
`ART_SCALE` (7) píxeles de pantalla.
