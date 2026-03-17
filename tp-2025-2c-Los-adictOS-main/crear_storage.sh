#!/bin/bash

# Script para crear directorio storage con superblock.config
# Uso: ./crear_storage.sh

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=== Creación de Directorio Storage ===${NC}\n"

# Preguntar por el directorio donde crear storage
read -p "¿Dónde quieres crear el directorio storage? (ruta completa, ej: /home/utnso): " BASE_DIR

if [ -z "$BASE_DIR" ]; then
    echo -e "${RED}Error: Debes especificar una ruta${NC}"
    exit 1
fi

STORAGE_DIR="${BASE_DIR}/storage"

# Verificar si ya existe
if [ -d "$STORAGE_DIR" ]; then
    echo -e "${YELLOW}El directorio $STORAGE_DIR ya existe${NC}"
    read -p "¿Deseas eliminarlo y crear uno nuevo? (s/n): " respuesta
    if [ "$respuesta" = "s" ] || [ "$respuesta" = "S" ]; then
        rm -rf "$STORAGE_DIR"
        echo -e "${GREEN}✓ Directorio eliminado${NC}"
    else
        echo -e "${RED}Operación cancelada${NC}"
        exit 0
    fi
fi

# Crear directorio storage
mkdir -p "$STORAGE_DIR"
echo -e "${GREEN}✓ Directorio creado: $STORAGE_DIR${NC}"

# Pedir valores para superblock.config
echo ""
read -p "FS_SIZE (tamaño del filesystem en bytes, ej: 8192): " FS_SIZE
read -p "BLOCK_SIZE (tamaño de bloque en bytes, ej: 16): " BLOCK_SIZE

# Validar que sean números
if ! [[ "$FS_SIZE" =~ ^[0-9]+$ ]] || ! [[ "$BLOCK_SIZE" =~ ^[0-9]+$ ]]; then
    echo -e "${RED}Error: FS_SIZE y BLOCK_SIZE deben ser números${NC}"
    rm -rf "$STORAGE_DIR"
    exit 1
fi

# Crear superblock.config
SUPERBLOCK_FILE="${STORAGE_DIR}/superblock.config"

cat > "$SUPERBLOCK_FILE" << EOF
FS_SIZE=$FS_SIZE
BLOCK_SIZE=$BLOCK_SIZE
EOF

echo ""
echo -e "${GREEN}✓ Archivo creado: $SUPERBLOCK_FILE${NC}"
echo ""
echo -e "${YELLOW}Contenido del superblock.config:${NC}"
cat "$SUPERBLOCK_FILE"
echo ""
echo -e "${GREEN}✓ Storage creado exitosamente en: $STORAGE_DIR${NC}"