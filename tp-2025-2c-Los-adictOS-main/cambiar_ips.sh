#!/bin/bash

# Script para cambiar configuraciones en archivos .config
# Uso: ./cambiar_configs.sh

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}=== Script de Cambio de Configuraciones ===${NC}\n"

# IMPORTANTE: Cambiar esta ruta al directorio de tu proyecto
PROJECT_DIR="/home/utnso/tp-2025-2c-Los-adictOS"

if [ ! -d "$PROJECT_DIR" ]; then
    echo -e "${RED}Error: No se encontró el directorio del proyecto${NC}"
    echo "Edita este script y cambia PROJECT_DIR a la ruta correcta"
    exit 1
fi

cambiar_config() {
    local VARIABLE=$1
    local NUEVO_VALOR=$2
    local contador=0
    
    echo -e "${YELLOW}Buscando archivos con $VARIABLE...${NC}"
    
    for archivo in $(find "$PROJECT_DIR" -name "*.config" -type f); do
        if grep -q "^${VARIABLE}=" "$archivo" 2>/dev/null; then
            # Reemplazar el valor directamente (sin backup)
            sed -i "s|^${VARIABLE}=.*|${VARIABLE}=${NUEVO_VALOR}|" "$archivo"
            
            echo -e "  ${GREEN}✓${NC} $archivo"
            contador=$((contador + 1))
        fi
    done
    
    if [ $contador -eq 0 ]; then
        echo -e "  ${YELLOW}No se encontraron archivos con $VARIABLE${NC}"
    else
        echo -e "${GREEN}Total actualizado: $contador archivo(s)${NC}"
    fi
}

while true; do
    echo ""
    echo -e "${GREEN}¿Qué configuración deseas cambiar?${NC}"
    echo "1) IP_MASTER"
    echo "2) IP_STORAGE"
    echo "3) PUNTO_MONTAJE"
    echo "4) PATH_QUERIES"
    echo "5) Salir"
    echo ""
    read -p "Opción [1-5]: " opcion
    
    case $opcion in
        1)
            read -p "Nueva IP para IP_MASTER: " nuevo_valor
            if [[ $nuevo_valor =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]]; then
                cambiar_config "IP_MASTER" "$nuevo_valor"
                echo -e "${GREEN}✓ IP_MASTER actualizada a $nuevo_valor${NC}"
            else
                echo -e "${RED}Error: IP inválida (formato esperado: xxx.xxx.xxx.xxx)${NC}"
            fi
            ;;
        2)
            read -p "Nueva IP para IP_STORAGE: " nuevo_valor
            if [[ $nuevo_valor =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]]; then
                cambiar_config "IP_STORAGE" "$nuevo_valor"
                echo -e "${GREEN}✓ IP_STORAGE actualizada a $nuevo_valor${NC}"
            else
                echo -e "${RED}Error: IP inválida (formato esperado: xxx.xxx.xxx.xxx)${NC}"
            fi
            ;;
        3)
            echo -e "${BLUE}Ejemplo: /home/utnso/storage${NC}"
            read -p "Nuevo PUNTO_MONTAJE: " nuevo_valor
            if [ -z "$nuevo_valor" ]; then
                echo -e "${RED}Error: El punto de montaje no puede estar vacío${NC}"
            else
                # Eliminar slash final si existe
                nuevo_valor="${nuevo_valor%/}"
                cambiar_config "PUNTO_MONTAJE" "$nuevo_valor"
                echo -e "${GREEN}✓ PUNTO_MONTAJE actualizado a $nuevo_valor${NC}"
            fi
            ;;
        4)
            echo -e "${BLUE}Ejemplo: /home/utnso/queries.txt${NC}"
            read -p "Nuevo PATH_QUERIES: " nuevo_valor
            if [ -z "$nuevo_valor" ]; then
                echo -e "${RED}Error: El path de queries no puede estar vacío${NC}"
            else
                cambiar_config "PATH_QUERIES" "$nuevo_valor"
                echo -e "${GREEN}✓ PATH_QUERIES actualizado a $nuevo_valor${NC}"
            fi
            ;;
        5)
            echo -e "${GREEN}¡Hasta luego!${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}Opción inválida${NC}"
            ;;
    esac
done
